use std::collections::HashSet;
use std::net::{IpAddr, Ipv4Addr};
use std::time::{SystemTime, UNIX_EPOCH};
use std::{collections::HashMap, os::unix::fs::PermissionsExt, path::PathBuf, process, sync::Arc};
use std::result::Result::Ok;
use futures::{future, prelude::*};
use dc::DC;
use local_ip_address::local_ip;
use rand::distributions::Alphanumeric;
use rand::Rng;
use signal_hook::{consts::TERM_SIGNALS, iterator::Signals};
use tarpc::server::{self, Channel};
use tarpc::{
    context, server::incoming::Incoming, tokio_serde::formats::Json
};
use tokio::{io::AsyncWriteExt, task};
use tokio::fs::{remove_file, File};
use tokio::process::Command;
use tokio::sync::Mutex;
use std::path::Path;
#[derive(Clone)]
struct DCServer {
    path: Arc<Mutex<HashSet<PathBuf>>>,
    temp_paths: Arc<Mutex<Vec<String>>>,
    data: Arc<Mutex<HashMap<String, String>>>,
}

async fn spawn(fut: impl Future<Output = ()> + Send + 'static) {
    tokio::spawn(fut);
}

fn generate_random_filename() -> String {
    let random_string: String = rand::thread_rng()
        .sample_iter(&Alphanumeric)
        .take(10)
        .map(char::from)
        .collect();

    // Get the current timestamp in seconds
    let duration_since_epoch = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("Time went backwards");
    let timestamp = duration_since_epoch.as_secs();

    // Combine random string, timestamp, and the provided extension
    format!("{}_{}", random_string, timestamp)
}

impl DC for DCServer {
    async fn hello(self, _: context::Context) -> String {
        return "Hello".to_string();
    }
    async fn send_binary(self, _: context::Context, binary: Vec<u8>) -> String {
        let filename = format!("/tmp/{}", generate_random_filename());
        println!("{}", filename);
        let mut file = File::create(&filename).await.unwrap();
        let metadata = file.metadata().await.expect("Failed to get metadata");
        let mut permissions = metadata.permissions();
        permissions.set_mode(0o755);
        tokio::fs::set_permissions(&filename, permissions).await.expect("Failed to set permissions");
        file.write_all(&binary).await.unwrap();
        file.flush().await.unwrap();
        tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;
        self.path.lock().await.insert(filename.clone().into());
        return filename.to_string();
    }
    async fn remove_binary(self, _: context::Context, filename: String) -> String {
        let mut data = self.path.lock().await;

        if data.contains(&PathBuf::from(&filename)) {
            match remove_file(&filename).await {
                Ok(_) => {
                    data.remove(&PathBuf::from(&filename));
                    "Binary removed successfully".to_string()
                },
                Err(e) => format!("Failed to remove {}: {}", filename, e),
            }
        } else {
            "Binary not in data".to_string()
        }
    }
    async fn some_binary(self, _: context::Context, name: String) -> bool {
        let file_name = format!("/tmp/{}", name);
        let path = Path::new(&file_name);
        path.exists()
    }
    async fn execute_binary(self, _: context::Context, filename: String, args: Vec<String>, stdin_input: Vec<u8>) -> Vec<u8> {
        if self.path.lock().await.contains(&PathBuf::from(&filename)) {
            let mut child = Command::new(filename)
                .args(args)
                .stdin(std::process::Stdio::piped())
                .stdout(std::process::Stdio::piped())
                .stderr(std::process::Stdio::piped())
                .kill_on_drop(true)
                .spawn()
                .unwrap();
            if let Some(mut stdin) = child.stdin.take() {
                stdin.write_all(&stdin_input).await.expect("Failed to write to stdin");
            }
            let output = child.wait_with_output().await.unwrap();
            println!("Execution done");
            if output.status.success() {
                return output.stdout;
            } else {
                return output.stderr;
            }
        } else {
            return b"Error, exec not in scope".to_vec();
        }
    }
    async fn stop_execution(self, _: context::Context) -> bool {
        false
    }
    async fn put(self, _: context::Context, key: String, value: String) -> String {
        self.data.lock().await.insert(key, value);
        return "Value Added".to_string();
    }
    async fn append(self, _: context::Context, key: String, value: String) -> String {
        let mut data = self.data.lock().await;
        if data.contains_key(&key) {
            let msg = format!("Appended Key, Old Value: {}", data.get(&key).unwrap());
            data.remove(&key);
            data.insert(key, value);
            return msg;
        }
        return "Error: Key does not exist".to_string();
    }
    async fn get(self, _: context::Context, key: String) -> String {
        let data = self.data.lock().await;
        if data.contains_key(&key) {
            return format!("Value: {}", data.get(&key).unwrap());
        }
        return "Error: Key does not exist".to_string();
    }
    async fn store_in_tmp(self, _: context::Context, key: String) -> String {
        let data = self.data.lock().await;
        if data.contains_key(&key) {
            let data = data.get(&key).unwrap();
            let file_name = format!("/tmp/{}", key.clone());
            let mut file = File::create(file_name.clone()).await.unwrap();
            file.write_all(data.as_bytes()).await.unwrap();
            file.flush().await.unwrap();
            tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;
            self.temp_paths.lock().await.push(key.clone());
            return format!("Value: stored in /tmp/{}", &key);
        }
        return "Error: Key does not exist".to_string();
    }
    async fn delete_in_tmp(self, _: context::Context, key: String) -> String {
        let data = self.temp_paths.lock().await;
        if data.contains(&key) {
            if let Err(e) = tokio::fs::remove_file(format!("/tmp/{}", key)).await {
                return format!("Failed to delete /tmp/{}: {}", &key, e);
            } else {
                return format!("Deleted /tmp/{}", &key);
            }
        }
        return "Error: Key is not stored in tmp".to_string();
    }
    async fn delete_tmp(self, _: context::Context) -> String {
        let data = self.temp_paths.lock().await;
        for file in data.iter() {
            
            if let Err(e) = tokio::fs::remove_file(format!("/tmp/{}", file)).await {
                eprintln!("Failed to delete /tmp/{}: {}", file, e);
            } else {
                println!("Deleted /tmp/{}", file);
            }
        }
        return "Deleted all tmp files".to_string();
    }
}

#[tokio::main]
async fn main() -> std::io::Result<()> {

    let server = DCServer {
        path: Arc::new(Mutex::new(HashSet::new())),
        temp_paths: Arc::new(Mutex::new(vec![])),
        data: Arc::new(Mutex::new(HashMap::new())),
    };
    let server = Arc::new(server);
    let server_clone = Arc::clone(&server);
    task::spawn(async move {
        let mut signals = Signals::new(TERM_SIGNALS).expect("Failed to create signal handler");
        for sig in signals.forever() {
            println!("Received signal {:?}", sig);
            for exe in server_clone.path.lock().await.iter() {
                let file_name = exe.to_str().unwrap();
                if let Err(e) = std::fs::remove_file(&file_name) {
                    eprintln!("Failed to delete {}: {}", file_name, e);
                } else {
                    println!("Deleted {}", file_name);
                }
            }
            for file_name in server_clone.temp_paths.lock().await.iter() {
                if let Err(e) = std::fs::remove_file(&file_name) {
                    eprintln!("Failed to delete {}: {}", file_name, e);
                } else {
                    println!("Deleted {}", file_name);
                }
            }
            
            process::exit(1);
        }
    });
    let server_addr = (IpAddr::V4(Ipv4Addr::LOCALHOST), 9010);

    let mut listener = tarpc::serde_transport::tcp::listen(&server_addr, Json::default).await?;
    println!("Listening on port {}", listener.local_addr().port());
    listener.config_mut().max_frame_length(usize::MAX);
    listener
        // Ignore accept errors.
        .filter_map(|r| future::ready(r.ok()))
        .map(server::BaseChannel::with_defaults)
        // Limit channels to 1 per IP.
        .max_channels_per_key(1, |t| t.transport().peer_addr().unwrap().ip())
        // serve is generated by the service attribute. It takes as input any type implementing
        // the generated World trait.
        .map(|channel| {
            channel.execute(<DCServer as Clone>::clone(&server).serve()).for_each(spawn)
        })
        // Max 10 channels.
        .buffer_unordered(10)
        .for_each(|_| async {})
        .await;
    println!("Serving On {}:9010", local_ip().unwrap());

    Ok(())
}
