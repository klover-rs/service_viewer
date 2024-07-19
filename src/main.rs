use std::ffi::CString;
use std::io::{self, Write};
use std::thread::sleep;
use std::time::Duration;
use colored::*;
use anyhow::Result;
use crossterm::event::{self, Event, KeyCode};

extern "C" {
    fn doesServiceExist(service_name: *const i8) -> bool;
    fn isServiceRunning(service_name: *const i8) -> bool;
    fn stopService(service_name: *const i8) -> bool;
}

#[link(name = "systemd")]
extern "C" {
    fn sd_bus_open_system(bus: *mut *mut std::ffi::c_void) -> i32;
    fn sd_bus_unref(bus: *mut std::ffi::c_void);
}

fn main() -> Result<()> {
    unsafe {
        let mut bus: *mut std::ffi::c_void = std::ptr::null_mut();
        sd_bus_open_system(&mut bus);
        sd_bus_unref(bus);
    }

    println!("{}", "Welcome to status viewer cli".bold());
    println!("please enter the name of your service to get details about it:\n");

    let mut input = String::new();

    io::stdout().flush()?;

    io::stdin().read_line(&mut input)?;


    let input = input.trim();

    println!("you entered: {}", &input);


    let c_service_name = if cfg!(target_os = "linux") {
        CString::new(format!("{}.service", &input))?
    } else {
        CString::new(input)?
    };

    let result = unsafe { doesServiceExist(c_service_name.as_ptr()) };

    if result {
        println!("{}", format!("service with name {} does exists", &input).green());
        let result = unsafe { isServiceRunning(c_service_name.as_ptr()) };
        if result {
            println!("{}", "service is running".green());
            
        } else {
            println!("{}", "service is not running".red());
        }
    } else {
        println!("{}", format!("No service found with name {}", &input).red());
    }

    println!("\npress enter to exit...");
    loop {
        if let Event::Key(key_event) = event::read()? {
            if key_event.code == KeyCode::Enter {
                break;
            }
        }
        sleep(Duration::from_millis(10));
    }

    Ok(())
    
}
