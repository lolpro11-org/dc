use tokio::io::{self, AsyncBufReadExt, BufReader};
use tokio::fs::File;
use std::path::Path;
use dc::DCClient;
use std::process;
use std::time::{Duration, Instant};
use signal_hook::{consts::TERM_SIGNALS, iterator::Signals};
use tarpc::{client, context, tokio_serde::formats::Json};
#[tokio::main]
async fn main() -> io::Result<()> {
    let systems = vec!["137.150.30.80"];
    let systems_clone = systems.clone();

    tokio::spawn(async move {
        let mut signals = Signals::new(TERM_SIGNALS).expect("Failed to create signal handler");
        for sig in signals.forever() {
            println!("Received signal {:?}", sig);
            for sys in &systems_clone {
                let server = tarpc::serde_transport::tcp::connect(format!("{}:9010", sys), Json::default).await.unwrap();
                let client = DCClient::new(client::Config::default(), server).spawn();

                let ctx = context::current();
                println!("{:?}", client.stop_execution(ctx).await.unwrap());
                println!("{:?}", client.delete_tmp(ctx).await.unwrap());
            }
            process::exit(1);
        }
    });

    for sys in &systems {
        let server = tarpc::serde_transport::tcp::connect(format!("{}:9010", sys), Json::default).await.unwrap();
        let client = DCClient::new(client::Config::default(), server).spawn();
        let mut ctx = context::current();
        ctx.deadline = Instant::now() + Duration::from_secs(10 * 60);
        for file in ["/usr/bin/ls", "/usr/bin/cp", "/usr/bin/lsblk"] {

            let binary_data = std::fs::read(file).unwrap();
            println!("{:?}", client.send_binary(ctx, binary_data).await.unwrap());
        }
    }
    process::exit(0);
    Ok(())
}

async fn read_first_line<P>(path: P) -> io::Result<String>
where
    P: AsRef<Path>,
{
    let file = File::open(path).await.unwrap();
    let reader = BufReader::new(file);
    if let Some(line_result) = reader.lines().next_line().await.unwrap() {
        return Ok(line_result);
    }
    Err(io::Error::new(io::ErrorKind::UnexpectedEof, "File is empty"))
}
