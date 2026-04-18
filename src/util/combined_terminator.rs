use crate::util::ctrlc_terminator::CtrlCTerminator;
use crate::util::ipc_terminator::IpcTerminator;
use crate::util::terminator::Terminator;
use jagua_rs::Instant;
use log::info;
use std::time::Duration;

/// Combined Terminator that listens for both IPC stop commands and Ctrl+C signals
/// This allows C++ to gracefully stop the optimization while also supporting Ctrl+C
#[derive(Debug, Clone)]
pub struct CombinedTerminator {
    pub ipc: IpcTerminator,
    pub ctrlc: CtrlCTerminator,
}

impl Default for CombinedTerminator {
    fn default() -> Self {
        Self::new()
    }
}

impl CombinedTerminator {
    /// Creates a new CombinedTerminator that handles both IPC and Ctrl+C
    pub fn new() -> Self {
        Self {
            ipc: IpcTerminator::new(),
            ctrlc: CtrlCTerminator::new(),
        }
    }
    
    /// Check if stop was requested via IPC
    pub fn is_ipc_stop_requested(&self) -> bool {
        self.ipc.is_stop_requested()
    }
    
    /// Check if Ctrl+C was triggered
    pub fn is_ctrlc_triggered(&self) -> bool {
        self.ctrlc.ctrlc.load(std::sync::atomic::Ordering::SeqCst)
    }
    
    /// Get the shutdown reason if any
    pub fn shutdown_reason(&self) -> Option<&'static str> {
        if self.is_ipc_stop_requested() {
            Some("Stop command received from C++")
        } else if self.is_ctrlc_triggered() {
            Some("Ctrl+C triggered")
        } else {
            None
        }
    }
}

impl Terminator for CombinedTerminator {
    fn kill(&self) -> bool {
        let ipc_requested = self.ipc.is_stop_requested();
        let ctrlc_triggered = self.ctrlc.ctrlc.load(std::sync::atomic::Ordering::SeqCst);
        let timeout_reached = self.ipc.timeout.is_some_and(|timeout| Instant::now() > timeout);
        
        // Debug: log kill() checks periodically
        if ipc_requested || ctrlc_triggered || timeout_reached {
            info!("[KILL CHECK] ipc={}, ctrlc={}, timeout={}", ipc_requested, ctrlc_triggered, timeout_reached);
        }
        
        // Return true if IPC stop requested
        if ipc_requested {
            info!("IPC stop requested by C++");
            return true;
        }
        
        // Return true if Ctrl+C was triggered
        if ctrlc_triggered {
            info!("Ctrl+C triggered");
            return true;
        }
        
        // Return true if timeout reached (check IPC's timeout)
        if timeout_reached {
            return true;
        }
        
        false
    }

    fn new_timeout(&mut self, timeout: Duration) {
        // Reset both terminators and set timeout on IPC
        self.ipc.new_timeout(timeout);
        // CtrlCTerminator doesn't have a reset method exposed, but we can rely on 
        // its timeout being set separately if needed
    }

    fn timeout_at(&self) -> Option<Instant> {
        self.ipc.timeout_at()
    }
}
