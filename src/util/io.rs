use crate::EPOCH;
use anyhow::{Context, Result};
use clap::Parser;
use jagua_rs::probs::spp::io::ext_repr::{ExtSPInstance, ExtSPSolution};
use log::{log, Level, LevelFilter};
use serde::{Deserialize, Serialize};
use std::fs;
use std::fs::File;
use std::io::BufReader;
use std::path::Path;
use svg::Document;

use std::io::{self, Read};

#[derive(Parser)]
pub struct MainCli {
    #[arg(long, default_value = "both")]
    pub output_mode: String, // file | stdout | both
    /// Path to input file (mandatory)
    #[arg(short = 'i', long, help = "Path to the input JSON file, or a solution JSON file for warm starting")]
    pub input: Option<String>,

    /// Global time limit in seconds (mutually exclusive with -e and -c)
    #[arg(short = 't', long, conflicts_with_all = &["exploration", "compression"], help = "Set a global time limit (in seconds)")]
    pub global_time: Option<u64>,

    /// Exploration time limit in seconds (requires compression time)
    #[arg(short = 'e', long, requires = "compression", help = "Set the exploration phase time limit (in seconds)")]
    pub exploration: Option<u64>,

    /// Compression time limit in seconds (requires exploration time)
    #[arg(short = 'c', long, requires = "exploration", help = "Set the compression phase time limit (in seconds)")]
    pub compression: Option<u64>,

    /// Enable early and automatic termination
    #[arg(short = 'x', long, help = "Enable early termination of the optimization process")]
    pub early_termination: bool,

    #[arg(short = 's', long, help = "Fixed seed for the random number generator")]
    pub rng_seed: Option<u64>,

    // ===== 新增采样参数 =====
    #[arg(long, help = "Number of container-wide samples per placement")]
    pub container_samples: Option<usize>,
    
    #[arg(long, help = "Number of focused samples around reference placement")]
    pub focused_samples: Option<usize>,
    
    #[arg(long, help = "Number of coordinate descents to perform")]
    pub coord_descents: Option<usize>,
 
    // ===== 新增分离器参数 =====
    #[arg(long, help = "Max iterations without improvement per strike")]
    pub iter_no_improve: Option<usize>,
    
    #[arg(long, help = "Strike limit for separator")]
    pub strike_limit: Option<usize>,
    
    #[arg(long, help = "Number of worker threads")]
    pub workers: Option<usize>,
 
    // ===== 新增探索阶段参数 =====
    #[arg(long, help = "Shrink step for exploration")]
    pub shrink_step: Option<f32>,
    
    #[arg(long, help = "Solution pool distribution stddev")]
    pub pool_stddev: Option<f32>,
 
    // ===== 新增压缩阶段参数 =====
    #[arg(long, help = "Shrink range min for compression")]
    pub shrink_min: Option<f32>,
    
    #[arg(long, help = "Shrink range max for compression")]
    pub shrink_max: Option<f32>,

    // ===== CDE 碰撞检测配置 =====
    #[arg(long, help = "Quadtree depth for collision detection")]
    pub quadtree_depth: Option<u8>,     
    
    #[arg(long, help = "Collision detection threshold")]
    pub cd_threshold: Option<u8>,       
    // ===== 几何处理配置 =====
    #[arg(long, help = "Polygon simplification tolerance (disabled if not set)")]
    pub poly_simpl_tolerance: Option<f32>,
    
    #[arg(long, help = "Minimum distance between items and hazards")]
    pub item_separation: Option<f32>,
    
    #[arg(long, help = "Narrow concavity cutoff distance")]
    pub concavity_dist: Option<f32>,
    
    #[arg(long, help = "Narrow concavity cutoff area ratio")]
    pub concavity_area: Option<f32>,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct ExtSPOutput {
    #[serde(flatten)]
    pub instance: ExtSPInstance,
    pub solution: ExtSPSolution,
}

pub fn init_logger(level_filter: LevelFilter, log_file_path: &Path) -> Result<()> {
    //remove old log file
    let _ = fs::remove_file(log_file_path);
    fern::Dispatch::new()
        // Perform allocation-free log formatting
        .format(|out, message, record| {
            let handle = std::thread::current();
            let thread_name = handle.name().unwrap_or("-");

            let duration = EPOCH.elapsed();
            let sec = duration.as_secs() % 60;
            let min = (duration.as_secs() / 60) % 60;
            let hours = (duration.as_secs() / 60) / 60;

            let prefix = format!(
                "[{}] [{:0>2}:{:0>2}:{:0>2}] <{}>",
                record.level(),
                hours,
                min,
                sec,
                thread_name,
            );

            out.finish(format_args!("{:<25}{}", prefix, message))
        })
        // Add blanket level filter -
        .level(level_filter)
        .chain(std::io::stderr())
        .chain(fern::log_file(log_file_path)?)
        .apply()?;
    log!(
        Level::Info,
        "[EPOCH]: {}",
        jiff::Timestamp::now()
    );
    Ok(())
}


pub fn write_svg(document: &Document, path: &Path, log_lvl: Level) -> Result<()> {
    //make sure the parent directory exists
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).context("could not create parent directory for svg file")?;
    }
    svg::save(path, document)?;
    log!(log_lvl,
        "[IO] svg exported to file://{}",
        fs::canonicalize(path)
            .expect("could not canonicalize path")
            .to_str()
            .unwrap()
    );
    Ok(())
}

pub fn write_json(json: &impl Serialize, path: &Path, log_lvl: Level) -> Result<()> {
    let file = File::create(path)?;
    serde_json::to_writer_pretty(file, json)?;
    log!(log_lvl,
        "[IO] json exported to file://{}",
        fs::canonicalize(path)
            .expect("could not canonicalize path")
            .to_str()
            .unwrap()
    );
    Ok(())
}

pub fn read_spp_input(path: &Path) -> Result<(ExtSPInstance, Option<ExtSPSolution>)> {
    let input_str = fs::read_to_string(path).context("could not read input file")?;
    //try parsing a full output (instance + solution)
    match serde_json::from_str::<ExtSPOutput>(&input_str) {
        Ok(ext_output) => {
            Ok((ext_output.instance, Some(ext_output.solution)))
        }
        Err(_) => {
            //try parsing just the instance
            let ext_instance = serde_json::from_str::<ExtSPInstance>(&input_str)
                .context("could not parse instance from input file")?;
            Ok((ext_instance, None))
        }
    }
}

pub fn read_spp_input_from_stdin() -> Result<(ExtSPInstance, Option<ExtSPSolution>)> {
    let mut input = String::new();
    io::stdin().read_to_string(&mut input)?;
    if let Ok(ext_output) = serde_json::from_str::<ExtSPOutput>(&input) {
        return Ok((ext_output.instance, Some(ext_output.solution)));
    }
    let ext_instance = serde_json::from_str::<ExtSPInstance>(&input)?;
    Ok((ext_instance, None))
}
