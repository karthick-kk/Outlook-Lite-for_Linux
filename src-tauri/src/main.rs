// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::fs::{self, File};

use nix::fcntl::{Flock, FlockArg};

fn main() {
    // Single-instance lock: ensure only one ofl process runs at a time
    let lock_dir = ofl_lib::config::data_dir();
    fs::create_dir_all(&lock_dir).expect("failed to create data directory");
    let lock_file = File::create(lock_dir.join("ofl.lock")).expect("failed to create lock file");

    let _lock = match Flock::lock(lock_file, FlockArg::LockExclusiveNonblock) {
        Ok(lock) => lock,
        Err((_, errno)) if errno == nix::errno::Errno::EWOULDBLOCK => {
            eprintln!("ofl is already running");
            std::process::exit(0);
        }
        Err((_, errno)) => {
            panic!("failed to acquire lock: {errno}");
        }
    };

    ofl_lib::run()
}
