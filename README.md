# service_viewer
A CLI application to view quickly details about installed services on your system!
It's made using a combination of system level apis in C++ and C and using FFI to combine it with rust.

## Support
Current only windows is supported, linux is in development. And MacOS is planned as well, just be patient as i work on those implementations. 

## How to build 
### requirements
- a C compiler (on windows msvc)
- [Rust lang](https://www.rust-lang.org/tools/install)

1. Clone the repo
2. navigate into it using your prefered terminal
3. `cargo run`

## Precompiled versions
Those can be found in the release tab, more precompiled version are about to follow.

## Known issues
On windows, if you build the project with `cargo build --release` and try to run the rust generated exe, the application will crash. fortunately, `cargo build` works tho

## Usage 
enter the name of your service (**IMPORTANT** the service name is *not* the display name of the service)

if you want to view all services you can enter in the console input `:all_services` 

on Windows, you might need to run the application with adminstrator, or else some processes might not be rendered due of not sufficient privileges 

![image](https://github.com/user-attachments/assets/3d477f20-61ed-4525-bfb8-d15e5d0fc32e)
![image](https://github.com/user-attachments/assets/451a0500-16a8-4278-9071-ed9cf48340bc)
