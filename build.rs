fn main() {

    if cfg!(target_os = "linux") {
        let lib_dir = "/usr/lib/x86_64-linux-gnu";
        println!("cargo:rustc-link-search=native={}", lib_dir);
        println!("cargo:rustc-link-lib=dylib=systemd");
        cc::Build::new()
            .file("src/service.c")
            .include("/usr/include/systemd")
            .flag("-lsystemd")
            .compile("service");

    } else {
        cc::Build::new()
            .cpp(true)
            .file("src/service.cpp")
            .compile("service");
    }


    println!("cargo:rerun-if-changed=src/service.c"); 
    println!("cargo:rerun-if-env-changed=CC");
    println!("cargo:rustc-link-lib=static=service");

}