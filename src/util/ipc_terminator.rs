use crate::util::terminator::Terminator;
use jagua_rs::Instant;
use log::{info, warn};
use std::fs;
use std::path::Path;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

/// IPC-based Terminator that listens for stop commands from C++ via a control file
/// Protocol: C++ creates a file at the specified path to request graceful shutdown
/// 
/// This avoids conflicts with stdin which is used for input data.
/// 
/// Default control file: "stop_signal.txt" in current directory
#[derive(Debug, Clone)]
pub struct IpcTerminator {
    pub timeout: Option<Instant>,
    stop_requested: Arc<AtomicBool>,
    control_file: String,
}

impl Default for IpcTerminator {
    fn default() -> Self {
        Self::new()
    }
}

impl IpcTerminator {
    /// Creates a new IpcTerminator with default control file path
    pub fn new() -> Self {
        Self::with_control_file("stop_signal.txt")
    }
    
    /// Creates a new IpcTerminator with a custom control file path
    pub fn with_control_file<P: AsRef<Path>>(path: P) -> Self {
        let stop_requested = Arc::new(AtomicBool::new(false));
        let stop_flag = stop_requested.clone();
        let control_file = path.as_ref().to_string_lossy().to_string();
        let control_file_clone = control_file.clone();
        
        // Spawn thread to poll for control file
        thread::spawn(move || {
            let check_interval = Duration::from_millis(100);
            let path = Path::new(&control_file_clone);
            
            info!("[IPC] Monitoring control file: {:?}", path);
            
            let mut check_count = 0;
            loop {
                check_count += 1;
                
                // Check if control file exists
                let exists = path.exists();
                if check_count % 50 == 0 {  // 每5秒记录一次
                    info!("[IPC] Checking control file (check #{}, exists={})", check_count, exists);
                }
                
                if exists {
                    info!("[IPC] Stop control file detected: {}", control_file_clone);
                    stop_flag.store(true, Ordering::SeqCst);
                    
                    // Optionally read and delete the file
                    match fs::read_to_string(path) {
                        Ok(content) => {
                            info!("[IPC] Stop file content: {}", content.trim());
                            // Delete the file after reading
                            let _ = fs::remove_file(path);
                        }
                        Err(e) => {
                            warn!("[IPC] Could not read stop file: {}", e);
                        }
                    }
                    break;
                }
                
                // Check if stop was already requested (via other means)
                if stop_flag.load(Ordering::SeqCst) {
                    break;
                }
                
                thread::sleep(check_interval);
            }
        });
        
        Self {
            timeout: None,
            stop_requested,
            control_file,
        }
    }
    
    /// Request stop manually
    pub fn request_stop(&self) {
        info!("Manual stop requested");
        self.stop_requested.store(true, Ordering::SeqCst);
    }
    
    /// Check if stop was requested via IPC
    pub fn is_stop_requested(&self) -> bool {
        self.stop_requested.load(Ordering::SeqCst)
    }
    
    /// Get the control file path
    pub fn control_file(&self) -> &str {
        &self.control_file
    }
}

impl Terminator for IpcTerminator {
    fn kill(&self) -> bool {
        // Return true if any termination condition is met
        let timeout_reached = self.timeout.is_some_and(|timeout| Instant::now() > timeout);
        let stop_requested = self.stop_requested.load(Ordering::SeqCst);
        
        if stop_requested {
            info!("Graceful stop requested by C++, will complete current iteration...");
        }
        
        timeout_reached || stop_requested
    }

    fn new_timeout(&mut self, timeout: Duration) {
        // Set a new timeout, but DO NOT reset stop_requested
        // The IPC stop signal should persist across phase transitions (explore -> compress)
        self.timeout = Some(Instant::now() + timeout);
    }

    fn timeout_at(&self) -> Option<Instant> {
        self.timeout
    }
}
