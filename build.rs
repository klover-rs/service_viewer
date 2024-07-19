fn main() {
    let lib_dir = "/usr/lib/x86_64-linux-gnu";
    println!("cargo:rustc-link-search=native={}", lib_dir);
    println!("cargo:rustc-link-lib=dylib=systemd");

    println!("cargo:rerun-if-changed=src/service.c"); 
    println!("cargo:rerun-if-env-changed=CC");

    cc::Build::new()
        .file("src/service.c")
        .include("/usr/include/systemd")
        .flag("-lsystemd")
        .compile("mylib");

        println!("cargo:rustc-link-lib=static=mylib");

}