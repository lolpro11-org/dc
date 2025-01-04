use once_cell::sync::Lazy;
use tarpc::{client, context, serde_transport::tcp, tokio_serde::formats::Json};
use tokio::{runtime::{self, Runtime}, sync::RwLock};
use core::ffi::c_char;
use std::{collections::HashMap, ffi::{c_void, CStr, CString}, panic, sync::Arc};
static RUNTIME: Lazy<Runtime> = Lazy::new(|| {
    runtime::Builder::new_multi_thread().enable_all().build().expect("Failed to create Tokio runtime")
});

static CLIENTS: Lazy<RwLock<HashMap<String, Arc<DCClient>>>> = Lazy::new(|| {
    RwLock::new(HashMap::new())
});

#[tarpc::service]
pub trait DC {
    async fn hello() -> String;
    async fn send_binary(binary: Vec<u8>) -> String;
    async fn remove_binary(filename: String) -> String;
    async fn some_binary(name: String) -> bool;
    async fn execute_binary(filename: String, args: Vec<String>, stdin_input: Vec<u8>) -> Vec<u8>;
    async fn stop_execution() -> bool;
    async fn put(key: String, value: String) -> String;
    async fn append(key: String, value: String) -> String;
    async fn get(key: String) -> String;
    async fn store_in_tmp(key: String) -> String;
    async fn delete_in_tmp(key: String) -> String;
    async fn delete_tmp() -> String;
}

async fn get_client(ip_addr: String) -> Arc<DCClient> {
    {
        // Try to acquire a read lock
        let clients = CLIENTS.read().await;
        if let Some(client) = clients.get(&ip_addr) {
            return Arc::clone(client);
        }
    }

    // If client not found, create a new one
    let server = tcp::connect(format!("{}:9010", ip_addr), Json::default)
        .await
        .unwrap();
    let client = Arc::new(DCClient::new(client::Config::default(), server).spawn());

    // Acquire a write lock to store the new client
    {
        let mut clients = CLIENTS.write().await;
        clients.insert(ip_addr.clone(), Arc::clone(&client));
    }

    client
}

async fn hello(ip_addr: String) -> String {
    let client = get_client(ip_addr).await;
    let ctx = context::current();

    // Call the hello function on the client
    match client.hello(ctx).await {
        Ok(value) => value,
        Err(_) => "Error: Failed to execute hello".to_string(),
    }
}

#[no_mangle]
pub extern "C" fn c_hello(ip_addr: *const c_char) -> *const c_char {
    let result = panic::catch_unwind(|| {
        let hello_result = RUNTIME.block_on(hello(
            unsafe { CStr::from_ptr(ip_addr) }.to_string_lossy().into_owned()));

        hello_result
    });

    // Convert the result to a CString
    let message = match result {
        Ok(msg) => msg,
        Err(_) => "Error: Panic occurred in Rust function".to_string(),
    };

    // Create a CString from the message
    let c_string = CString::new(message).unwrap();
    c_string.into_raw()
}


async fn send_binary(ip_addr: String, binary: Vec<u8>) -> String {
    let client = get_client(ip_addr).await;
    let ctx = context::current();

    match client.send_binary(ctx, binary).await {
        Ok(value) => value,
        Err(_) => "Error: Failed to send binary".to_string(),
    }
}

#[no_mangle]
pub extern "C" fn c_send_binary(ip_addr: *const c_char, binary: *const u8, len: usize) -> *const c_char {
    let result = panic::catch_unwind(|| {
        let binary_vec = unsafe { std::slice::from_raw_parts(binary, len) }.to_vec();
        let ip_addr_str = unsafe { CStr::from_ptr(ip_addr) }.to_string_lossy().into_owned();
        let send_result = RUNTIME.block_on(send_binary(ip_addr_str, binary_vec));
        send_result
    });

    let message = match result {
        Ok(msg) => msg,
        Err(_) => "Error: Panic occurred in Rust function".to_string(),
    };

    let c_string = CString::new(message).unwrap();
    c_string.into_raw()
}

async fn some_binary(ip_addr: String, name: String) -> bool {
    let client = get_client(ip_addr).await;
    let ctx = context::current();

    client.some_binary(ctx, name).await.unwrap_or(false)
}

#[no_mangle]
pub extern "C" fn c_some_binary(ip_addr: *const c_char, name: *const c_char) -> bool {
    let result = panic::catch_unwind(|| 
        RUNTIME.block_on(some_binary(
            unsafe { CStr::from_ptr(ip_addr) }.to_string_lossy().into_owned(), 
            unsafe { CStr::from_ptr(name) }.to_string_lossy().into_owned()))
    );
    match result {
        Ok(value) => value,
        Err(_) => false,
    }
}

async fn remove_binary(ip_addr: String, filename: String) -> String {
    let client = get_client(ip_addr).await;
    let ctx = context::current();

    match client.remove_binary(ctx, filename).await {
        Ok(value) => value,
        Err(_) => "Error: Failed to remove binary".to_string(),
    }
}

#[no_mangle]
pub extern "C" fn c_remove_binary(ip_addr: *const c_char, filename: *const c_char) -> *const c_char {
    let result = panic::catch_unwind(|| 
        RUNTIME.block_on(remove_binary(
            unsafe { CStr::from_ptr(ip_addr) }.to_string_lossy().into_owned(), 
            unsafe { CStr::from_ptr(filename) }.to_string_lossy().into_owned()))
    );
    let message = match result {
        Ok(value) => value,
        Err(_) => "Error: Failed to remove binary".to_string(),
    };
    let c_string = CString::new(message).unwrap();
    c_string.into_raw()
}

async fn execute_binary(ip_addr: String, filename: String, args: Vec<String>, stdin_input: Vec<u8>) -> Vec<u8> {
    let client = get_client(ip_addr).await;
    let ctx = context::current();

    client.execute_binary(ctx, filename, args, stdin_input).await.unwrap_or_default()
}
#[no_mangle]
pub extern "C" fn c_execute_binary(
    ip_addr: *const c_char,
    filename: *const c_char,
    args: *const *const c_char,
    count: usize,
    stdin_input: *const u8,
    stdin_len: usize,
    out_len: *mut usize,
    out_cap: *mut usize
) -> *const u8 {
    let result = panic::catch_unwind(|| {
        // Convert `args` from C to Rust's Vec<String>
        let args_vec = (0..count)
            .map(|i| {
                let arg = unsafe { CStr::from_ptr(*args.add(i)) };
                arg.to_string_lossy().into_owned()
            })
            .collect::<Vec<String>>();

        let stdin_slice = unsafe { std::slice::from_raw_parts(stdin_input, stdin_len) };
        let stdin_vec = stdin_slice.to_vec();
        let ip_addr_str = unsafe { CStr::from_ptr(ip_addr) }.to_string_lossy().into_owned();
        let filename_str = unsafe { CStr::from_ptr(filename) }.to_string_lossy().into_owned();
        // Run the async function and wait for the result
        RUNTIME.block_on(execute_binary(ip_addr_str, filename_str, args_vec, stdin_vec))
    });

    // Handle the result and convert it to raw output
    let binary_data = match result {
        Ok(data) => data,
        Err(_) => Vec::new(),
    };

    unsafe { *out_len = binary_data.len() };
    unsafe { *out_cap = binary_data.capacity() };
    binary_data.leak().as_ptr()
}

async fn stop_execution(ip_addr: String) -> bool {
    let client = get_client(ip_addr).await;
    let ctx = context::current();

    client.stop_execution(ctx).await.unwrap_or(false)
}

#[no_mangle]
pub extern "C" fn c_stop_execution(ip_addr: *const c_char) -> bool {
    let result = panic::catch_unwind(|| RUNTIME.block_on(stop_execution(unsafe { CStr::from_ptr(ip_addr) }.to_string_lossy().into_owned())));
    match result {
        Ok(value) => value,
        Err(_) => false,
    }
}

async fn put(ip_addr: String, key: String, value: String) -> String {
    let client = get_client(ip_addr).await;
    let ctx = context::current();

    client.put(ctx, key, value).await.unwrap_or("Error: Failed to put data".to_string())
}

#[no_mangle]
pub extern "C" fn c_put(ip_addr: *const c_char, key: *const c_char, value: *const c_char) -> *const c_char {
    let result = panic::catch_unwind(|| {
        let key_str = unsafe { CStr::from_ptr(key) }.to_string_lossy().into_owned();
        let value_str = unsafe { CStr::from_ptr(value) }.to_string_lossy().into_owned();
        let ip_addr_str = unsafe { CStr::from_ptr(ip_addr) }.to_string_lossy().into_owned();
        RUNTIME.block_on(put(ip_addr_str, key_str, value_str))
    });

    let message = match result {
        Ok(msg) => msg,
        Err(_) => "Error: Panic occurred in Rust function".to_string(),
    };

    let c_string = CString::new(message).unwrap();
    c_string.into_raw()
}

async fn append(ip_addr: String, key: String, value: String) -> String {
    let client = get_client(ip_addr).await;
    let ctx = context::current();

    client.append(ctx, key, value).await.unwrap_or("Error: Failed to append data".to_string())
}

#[no_mangle]
pub extern "C" fn c_append(ip_addr: *const c_char, key: *const c_char, value: *const c_char) -> *const c_char {
    let result = panic::catch_unwind(|| {
        let key_str = unsafe { CStr::from_ptr(key) }.to_string_lossy().into_owned();
        let value_str = unsafe { CStr::from_ptr(value) }.to_string_lossy().into_owned();
        let ip_addr_str = unsafe { CStr::from_ptr(ip_addr) }.to_string_lossy().into_owned();
        RUNTIME.block_on(append(ip_addr_str, key_str, value_str))
    });

    let message = match result {
        Ok(msg) => msg,
        Err(_) => "Error: Panic occurred in Rust function".to_string(),
    };

    let c_string = CString::new(message).unwrap();
    c_string.into_raw()
}

async fn get(ip_addr: String, key: String) -> String {
    let client = get_client(ip_addr).await;
    let ctx = context::current();

    client.get(ctx, key).await.unwrap_or("Error: Failed to get data".to_string())
}

#[no_mangle]
pub extern "C" fn c_get(ip_addr: *const c_char, key: *const c_char) -> *const c_char {
    let result = panic::catch_unwind(|| {
        let key_str = unsafe { CStr::from_ptr(key) }.to_string_lossy().into_owned();
        let ip_addr_str = unsafe { CStr::from_ptr(ip_addr) }.to_string_lossy().into_owned();
        RUNTIME.block_on(get(ip_addr_str, key_str))
    });

    let message = match result {
        Ok(msg) => msg,
        Err(_) => "Error: Panic occurred in Rust function".to_string(),
    };

    let c_string = CString::new(message).unwrap();
    c_string.into_raw()
}

async fn store_in_tmp(ip_addr: String, key: String) -> String {
    let client = get_client(ip_addr).await;
    let ctx = context::current();

    client.store_in_tmp(ctx, key).await.unwrap_or("Error: Failed to store in tmp".to_string())
}

#[no_mangle]
pub extern "C" fn c_store_in_tmp(ip_addr: *const c_char, key: *const c_char) -> *const c_char {
    let result = panic::catch_unwind(|| {
        let key_str = unsafe { CStr::from_ptr(key) }.to_string_lossy().into_owned();
        let ip_addr_str = unsafe { CStr::from_ptr(ip_addr) }.to_string_lossy().into_owned();
        RUNTIME.block_on(store_in_tmp(ip_addr_str, key_str))
    });

    let message = match result {
        Ok(msg) => msg,
        Err(_) => "Error: Panic occurred in Rust function".to_string(),
    };

    let c_string = CString::new(message).unwrap();
    c_string.into_raw()
}

async fn delete_in_tmp(ip_addr: String, key: String) -> String {
    let client = get_client(ip_addr).await;
    let ctx = context::current();

    client.delete_in_tmp(ctx, key).await.unwrap_or("Error: Failed to delete in tmp".to_string())
}

#[no_mangle]
pub extern "C" fn c_delete_in_tmp(ip_addr: *const c_char, key: *const c_char) -> *const c_char {
    let result = panic::catch_unwind(|| {
        let key_str = unsafe { CStr::from_ptr(key) }.to_string_lossy().into_owned();
        let ip_addr_str = unsafe { CStr::from_ptr(ip_addr) }.to_string_lossy().into_owned();
        RUNTIME.block_on(delete_in_tmp(ip_addr_str, key_str))
    });

    let message = match result {
        Ok(msg) => msg,
        Err(_) => "Error: Panic occurred in Rust function".to_string(),
    };

    let c_string = CString::new(message).unwrap();
    c_string.into_raw()
}

async fn delete_tmp(ip_addr: String) -> String {
    let client = get_client(ip_addr).await;
    let ctx = context::current();

    client.delete_tmp(ctx).await.unwrap_or("Error: Failed to delete tmp".to_string())
}

#[no_mangle]
pub extern "C" fn c_delete_tmp(ip_addr: *const c_char) -> *const c_char {
    let result = panic::catch_unwind(|| RUNTIME.block_on(delete_tmp(
        unsafe { CStr::from_ptr(ip_addr) }.to_string_lossy().into_owned())));

    let message = match result {
        Ok(msg) => msg,
        Err(_) => "Error: Panic occurred in Rust function".to_string(),
    };

    let c_string = CString::new(message).unwrap();
    c_string.into_raw()
}

#[no_mangle]
pub extern "C" fn free_string(ptr: *const c_char) {
    if ptr.is_null() { return; }
    unsafe { let _ = CString::from_raw(ptr as *mut c_char); }
}

#[no_mangle]
pub extern "C" fn free_vec(ptr: *mut c_void, length: usize, capacity: usize) {
    if ptr.is_null() {
        return;
    }
    unsafe { Vec::from_raw_parts(ptr as *mut u8, length, capacity); }
}
