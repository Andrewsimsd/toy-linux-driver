use toy_linux_driver::{DEVICE_PATH, parse_config, run_loop};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().collect();
    let config = parse_config(&args)?;

    eprintln!(
        "starting sender loop: device={DEVICE_PATH}, delay_ms={}, iterations={:?}",
        config.delay_ms, config.iterations
    );

    run_loop(DEVICE_PATH, config)?;
    Ok(())
}
