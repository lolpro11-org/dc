use tokio::process::Command;
use tokio::io::{self, AsyncBufReadExt, BufReader};
use tokio::fs::File;
use std::path::Path;
use dc::DCClient;
use std::process;
use std::time::{Duration, Instant};
use futures::{stream::FuturesUnordered, StreamExt};
use signal_hook::{consts::TERM_SIGNALS, iterator::Signals};
use tarpc::{client, context, tokio_serde::formats::Json};
use tokio::task;
use tokio::io::AsyncWriteExt;
use zerocopy::IntoBytes;

fn get_std_from_char(c: char) -> f64 {
    let mass_e = 1.0;
    let mass_d = 3670.0;
    let mass_t = 5467.0;
    let mass_h = 7289.0;

    match c {
        'e' => 1.0,
        'd' => (mass_e as f64 / mass_d).sqrt(),
        't' => (mass_e as f64 / mass_t).sqrt(),
        'h' => (mass_e as f64 / mass_h).sqrt(),
        _ => 1.0,
    }
}

fn get_mass_from_char(c: char) -> f64 {
    match c {
        'e' => 1.0,
        'd' => 3670.0,
        't' => 5467.0,
        'h' => 7289.0,
        _ => 1.0,
    }
}

fn get_charge_from_char(c: char) -> f64 {
    match c {
        'e' => -1.0,
        'd' => 1.0,
        't' => 1.0,
        'h' => 2.0,
        _ => 1.0,
    }
}

#[tokio::main]
async fn main() -> io::Result<()> {
    let systems = vec!["192.168.1.21", "192.168.1.22", "192.168.1.23", "192.168.1.24"];
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
    let n: i64 = 100_000;
    let num_timesteps: i64 = 3;

    let dt = 0.01;
    let l = 1.0;

    let mass_e = 1.0;
    let mass_d = 3670.0;
    let mass_t = 5467.0;
    let mass_h = 7289.0;
    let deuterium_std = (mass_e as f64 / mass_d).sqrt();
    let tritium_std = (mass_e as f64 / mass_t).sqrt();
    let alpha_std = (mass_e as f64 / mass_h).sqrt();
    let external_electric_field = 50.0;

    let blocks_per_batch: i64 = 32768;
    let threads_per_block: i64 = 1024;
    let threads_per_batch = blocks_per_batch * threads_per_block;

    let j = (250.0 * l) as i64;
    let n_0 = 5.0e20;
    let eps_0 = 8.85e-12;
    let boltzmann_constant = 1.38e-23;
    let electron_charge = 1.60e-19;
    let electron_mass = 9.11e-31;
    let std_temperature = 3e8;
    let debye_length = (eps_0 * std_temperature * boltzmann_constant / (n_0 * ((electron_charge as f64).powi(2)))).sqrt();
    let plasma_period = (eps_0 * electron_mass / (n_0 * ((electron_charge as f64).powi(2)))).sqrt();
    let n_e = n_0 * debye_length.powi(3);
    let n_01d = n_e.powf(1.0 / 3.0);
    let c = 1.0 / (4.0 * std::f64::consts::PI * n_e);
    
    let plasma_period_commands_args = [
        ["e1.txt".to_string(), n.to_string(), l.to_string(), 1.to_string(), 1.to_string()],
        ["e2.txt".to_string(), n.to_string(), l.to_string(), 1.to_string(), 1.to_string()],
        ["d.txt".to_string(), n.to_string(), l.to_string(), deuterium_std.to_string(), 1.to_string()],
        ["t.txt".to_string(), n.to_string(), l.to_string(), tritium_std.to_string(), 1.to_string()],
        ["h.txt".to_string(), n.to_string(), l.to_string(), alpha_std.to_string(), 0.to_string()],
    ];

    for cmdargs in plasma_period_commands_args {
        if Command::new("bash")
                .arg("-c")
                .arg(format!("./bin/generatePlasma.out {}", cmdargs.to_vec().join(" ")))
                .output()
                .await.unwrap().status.success() {
            println!("plasmagen done successfully!");
        } else {
            eprintln!("Error!");
        }
    }
    if Command::new("bash")
            .arg("-c")
            .arg(format!("touch ./fusions.txt"))
            .output()
            .await.unwrap().status.success() {
        println!("plasmagen done successfully!");
    } else {
        eprintln!("Error!");
    }

    let mut futs = FuturesUnordered::new();
    let mut outputs = Vec::new();
    let mut binary_names = vec![];
    for sys in &systems {
        let server = tarpc::serde_transport::tcp::connect(format!("{}:9010", sys), Json::default).await.unwrap();
        let client = DCClient::new(client::Config::default(), server).spawn();
        let mut ctx = context::current();
        ctx.deadline = Instant::now() + Duration::from_secs(10 * 60);
        let binary_data = std::fs::read("bin/collide.out").unwrap();
        let binary_name = client.send_binary(ctx, binary_data).await.unwrap();
        println!("{}", binary_name);
        binary_names.push(binary_name);
    }
    
    for timestep in 0..num_timesteps {
        let mut filenames = Vec::new();
        println!("Starting timestep {}", timestep);
        let sorting_commands_args = [
            ["e1sorted.txt".to_string(), "e1.txt".to_string(), n.to_string()],
            ["e2sorted.txt".to_string(), "e2.txt".to_string(), n.to_string()],
            ["dsorted.txt".to_string(), "d.txt".to_string(), n.to_string()],
            ["tsorted.txt".to_string(), "t.txt".to_string(), n.to_string()],
            ["hsorted.txt".to_string(), "h.txt".to_string(), n.to_string()],
        ];

        for cmdargs in sorting_commands_args {
            if Command::new("bash")
                .arg("-c")
                .arg(format!("./bin/sort.out {}", cmdargs.join(" ")))
                .output()
                .await.unwrap().status.success() {
                println!("sort done successfully!");
            } else {
                eprintln!("Error!");
            }
        }

        for collision_pair in 1..16 {
            let arrays = [
                ("e1", "e1"),
                ("e1", "e2"),
                ("e2", "e2"),
                ("e1", "d"),
                ("e1", "t"),
                ("e1", "h"),
                ("e2", "d"),
                ("e2", "t"),
                ("e2", "h"),
                ("d", "d"),
                ("d", "t"),
                ("d", "h"),
                ("t", "t"),
                ("t", "h"),
                ("h", "h"),
            ];

            let (first, second) = arrays[collision_pair as usize - 1];
            let self_collisions = first == second;
            let std1 = get_std_from_char(first.chars().next().unwrap());
            let std2 = get_std_from_char(second.chars().next().unwrap());

            Command::new("./bin/getSearchWidth.out")
                .args(["searchWidth.txt".to_string(), format!("{}sorted.txt", first), format!("{}sorted.txt", second), n.to_string(), l.to_string(), dt.to_string(), std1.to_string(), std2.to_string(), plasma_period.to_string(), debye_length.to_string()].to_vec())
                .stdin(std::process::Stdio::piped())
                .stdout(std::process::Stdio::piped())
                .stderr(std::process::Stdio::piped())
                .kill_on_drop(true)
                .spawn()
                .unwrap()
                .wait_with_output().await.unwrap();

            let search_width_string = read_first_line("searchWidth.txt").await.unwrap();
            let search_width: i64 = search_width_string.trim().parse().unwrap();
            println!("Search_width {}", search_width);
            let num_batches = (n * search_width * (2 - self_collisions as i64) + threads_per_batch - 1) / threads_per_batch;
            for batch in 0..num_batches {
                let sys = systems[batch as usize % systems.len()];
                let binary_name = binary_names[batch as usize % systems.len()].clone();
                let filename = format!("collide{}{}n{}.txt", first, second, batch);
                let filename_clone = filename.clone();

                let fut = async move {
                    let server = tarpc::serde_transport::tcp::connect(format!("{}:9010", sys), Json::default).await.unwrap();
                    let client = DCClient::new(client::Config::default(), server).spawn();
                    let mut ctx = context::current();
                    ctx.deadline = Instant::now() + Duration::from_secs(10 * 60);

                    let binding = client.execute_binary(ctx, binary_name, [
                        filename_clone.clone(), 
                        format!("{}/{}sorted.txt", std::env::current_dir().unwrap().to_str().unwrap(), first), 
                        format!("{}/{}sorted.txt", std::env::current_dir().unwrap().to_str().unwrap(), second), 
                        get_mass_from_char(first.chars().next().unwrap()).to_string(),
                        get_mass_from_char(second.chars().next().unwrap()).to_string(),
                        get_charge_from_char(first.chars().next().unwrap()).to_string(),
                        get_charge_from_char(second.chars().next().unwrap()).to_string(),
                        c.to_string(), dt.to_string(), (batch % 2).to_string(), search_width.to_string(),
                        n.to_string(), batch.to_string(), threads_per_block.to_string(), blocks_per_batch.to_string(),
                        collision_pair.to_string()
                    ].to_vec(), vec![]).await.unwrap();

                    let mut file = File::create(&filename_clone).await.unwrap();
                    println!("Writing to file: {}", filename_clone);
                    file.write_all(binding.as_bytes()).await.unwrap();
                };

                futs.push(task::spawn(fut));
                if futs.len() == 32 {
                    outputs.push(futs.next().await.unwrap());
                }

                filenames.push(filename);
            }
        }

        while let Some(item) = futs.next().await {
            outputs.push(item);
        }
        if Command::new("bash")
            .arg("-c")
            .arg(format!("cat {} > unsortedCollisions.txt", filenames.join(" ")))
            .output()
            .await.unwrap().status.success() {
            println!("Files concatenated successfully!");
        } else {
            eprintln!("Error!");
        }
        if Command::new("bash")
            .arg("-c")
            .arg(format!("./bin/sortCollisions.out sortedCollisions.txt 1 unsortedCollisions.txt"))
            .output()
            .await.unwrap().status.success() {
            println!("Files sorted successfully!");
        } else {
            eprintln!("Error!");
        }
        if Command::new("bash")
            .arg("-c")
            .arg(format!("./bin/processCollisions.out {}", ["e1.txt".to_string(), "e2.txt".to_string(), "d.txt".to_string(), "t.txt".to_string(), "h.txt".to_string(), "e1.txt".to_string(), "e2.txt".to_string(), "d.txt".to_string(), "t.txt".to_string(), "h.txt".to_string(), n.to_string(), dt.to_string(), "sortedCollisions.txt".to_string(), debye_length.to_string(), plasma_period.to_string(), c.to_string(), timestep.to_string(), "./fusions.txt".to_string()].join(" ")))
            .output()
            .await.unwrap().status.success() {
            println!("collisions processed successfully!");
        } else {
            eprintln!("Error!");
        }
        if Command::new("bash")
            .arg("-c")
            .arg(format!("./bin/mover.out {}", ["e1.txt".to_string(), "e2.txt".to_string(), "d.txt".to_string(), "t.txt".to_string(), "h.txt".to_string(), "e1.txt".to_string(), "e2.txt".to_string(), "d.txt".to_string(), "t.txt".to_string(), "h.txt".to_string(), n.to_string(), j.to_string(), dt.to_string(), n_01d.to_string(), l.to_string(), external_electric_field.to_string()].join(" ")))
            .output()
            .await.unwrap().status.success() {
            println!("move done successfully!");
        } else {
            eprintln!("Error!");
        }
        println!("Finished Timestep: {}", timestep);
    }
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
