extern crate core;
use clap::Parser as Clap;
use jagua_rs::io::import::Importer;
use log::{info, warn, Level};
use rand::SeedableRng;
use sparrow::config::*;
use sparrow::optimizer::optimize;
use sparrow::util::io;
use sparrow::util::io::{ExtSPOutput, MainCli};
use sparrow::EPOCH;
use std::fs;
use std::path::Path;
use std::time::Duration;

use anyhow::{bail, Result};
use rand::rngs::Xoshiro256PlusPlus;
use sparrow::consts::{DEFAULT_COMPRESS_TIME_RATIO, DEFAULT_EXPLORE_TIME_RATIO, DEFAULT_FAIL_DECAY_RATIO_CMPR, DEFAULT_MAX_CONSEQ_FAILS_EXPL, LOG_LEVEL_FILTER_DEBUG, LOG_LEVEL_FILTER_RELEASE};
use sparrow::util::combined_terminator::CombinedTerminator;
use sparrow::util::svg_exporter::SvgExporter;
use sparrow::util::svg_exporter::SolverEvent;
use base64::{engine::general_purpose, Engine as _};
pub const OUTPUT_DIR: &str = "output";

pub const LIVE_DIR: &str = "data/live";

enum OutputMode {
    FileOnly,      // 只输出文件
    StdoutOnly,    // 只输出给 C++
    Both,          // 文件 + stdout
}

fn main() -> Result<()>{
    let mut config = DEFAULT_SPARROW_CONFIG;

    fs::create_dir_all(OUTPUT_DIR)?;
    let log_file_path = format!("{}/log.txt", OUTPUT_DIR);
    match cfg!(debug_assertions) {
        true => io::init_logger(LOG_LEVEL_FILTER_DEBUG, Path::new(&log_file_path))?,
        false => io::init_logger(LOG_LEVEL_FILTER_RELEASE, Path::new(&log_file_path))?,
    }

    let args = MainCli::parse();
    //let input_file_path = &args.input;
    let (explore_dur, compress_dur) = match (args.global_time, args.exploration, args.compression) {
        (Some(gt), None, None) => {
            (Duration::from_secs(gt).mul_f32(DEFAULT_EXPLORE_TIME_RATIO), Duration::from_secs(gt).mul_f32(DEFAULT_COMPRESS_TIME_RATIO))
        },
        (None, Some(et), Some(ct)) => {
            (Duration::from_secs(et), Duration::from_secs(ct))
        },
        (None, None, None) => {
            warn!("[MAIN] no time limit specified");
            (Duration::from_secs(600).mul_f32(DEFAULT_EXPLORE_TIME_RATIO), Duration::from_secs(600).mul_f32(DEFAULT_COMPRESS_TIME_RATIO))
        },
        _ => bail!("invalid cli pattern (clap should have caught this)"),
    };
    config.expl_cfg.time_limit = explore_dur;
    config.cmpr_cfg.time_limit = compress_dur;
    if args.early_termination {
        config.expl_cfg.max_conseq_failed_attempts = Some(DEFAULT_MAX_CONSEQ_FAILS_EXPL);
        config.cmpr_cfg.shrink_decay = ShrinkDecayStrategy::FailureBased(DEFAULT_FAIL_DECAY_RATIO_CMPR);
        warn!("[MAIN] early termination enabled!");
    }
    // 应用采样参数
    if let Some(n) = args.container_samples {
        config.expl_cfg.separator_config.sample_config.n_container_samples = n;
        config.cmpr_cfg.separator_config.sample_config.n_container_samples = n;
    }
    if let Some(n) = args.focused_samples {
        config.expl_cfg.separator_config.sample_config.n_focussed_samples = n;
        config.cmpr_cfg.separator_config.sample_config.n_focussed_samples = n;
    }
    if let Some(n) = args.coord_descents {
        config.expl_cfg.separator_config.sample_config.n_coord_descents = n;
        config.cmpr_cfg.separator_config.sample_config.n_coord_descents = n;
    }

    // 应用分离器参数
    if let Some(n) = args.iter_no_improve {
        config.expl_cfg.separator_config.iter_no_imprv_limit = n;
        config.cmpr_cfg.separator_config.iter_no_imprv_limit = n / 2; // 压缩阶段可减半
    }
    if let Some(n) = args.strike_limit {
        config.expl_cfg.separator_config.strike_limit = n;
        config.cmpr_cfg.separator_config.strike_limit = n + 2; // 压缩阶段可更宽松
    }
    if let Some(n) = args.workers {
        config.expl_cfg.separator_config.n_workers = n;
        config.cmpr_cfg.separator_config.n_workers = n;
    }

    // 应用探索阶段参数
    if let Some(v) = args.shrink_step {
        config.expl_cfg.shrink_step = v;
    }
    if let Some(v) = args.pool_stddev {
        config.expl_cfg.solution_pool_distribution_stddev = v;
    }

    // 应用压缩阶段参数
    if let (Some(min), Some(max)) = (args.shrink_min, args.shrink_max) {
        config.cmpr_cfg.shrink_range = (min, max);
    }
    // ===== CDE 配置 =====
    if let Some(d) = args.quadtree_depth {
        config.cde_config.quadtree_depth = d;
    }
    if let Some(t) = args.cd_threshold {
        config.cde_config.cd_threshold = t;
    }

    // ===== 几何处理配置 =====
    if let Some(v) = args.poly_simpl_tolerance {
        config.poly_simpl_tolerance = Some(v);
    }
    if let Some(v) = args.item_separation {
        config.min_item_separation = Some(v);
    }
    if let (Some(dist), Some(area)) = (args.concavity_dist, args.concavity_area) {
        config.narrow_concavity_cutoff_ratio = Some((dist, area));
    }
    // ===== RNG 种子 =====
    if let Some(arg_rng_seed) = args.rng_seed {
        config.rng_seed = Some(arg_rng_seed as usize);
    }

    info!("[MAIN] configured to explore for {}s and compress for {}s", explore_dur.as_secs(), compress_dur.as_secs());

    let rng = match config.rng_seed {
        Some(seed) => {
            info!("[MAIN] using seed: {}", seed);
            Xoshiro256PlusPlus::seed_from_u64(seed as u64)
        },
        None => {
            let seed = rand::random();
            warn!("[MAIN] no seed provided, using: {}", seed);
            Xoshiro256PlusPlus::seed_from_u64(seed)
        }
    };

    info!("[MAIN] system time: {}", jiff::Timestamp::now());

    //let (ext_instance, ext_solution) = io::read_spp_input(Path::new(&input_file_path))?;
    
    let input = args.input.as_deref().unwrap_or("-");
    let (ext_instance, ext_solution) = if input == "-" {
        io::read_spp_input_from_stdin()?
    } else {
        io::read_spp_input(Path::new(&input))?
    };

    let importer = Importer::new(config.cde_config, config.poly_simpl_tolerance, config.min_item_separation, config.narrow_concavity_cutoff_ratio);
    let instance = jagua_rs::probs::spp::io::import_instance(&importer, &ext_instance)?;

    let initial_solution = ext_solution.map(|e|
        jagua_rs::probs::spp::io::import_solution(&instance, &e)
    );

    info!("[MAIN] loaded instance {} with #{} items", ext_instance.name, instance.total_item_qty());
    let output_mode = match args.output_mode.as_str() {
        "file" => OutputMode::FileOnly,
        "stdout" => OutputMode::StdoutOnly,
        _ => OutputMode::Both,
    };
    
    let final_svg_path = match output_mode {
        OutputMode::StdoutOnly => None,
        _ =>  Some(format!("{OUTPUT_DIR}/final_{}.svg", ext_instance.name)),
    };

    let intermediate_svg_dir = if cfg!(feature = "only_final_svg") {
        None
    } else {
        match output_mode {
            OutputMode::StdoutOnly => None, //  禁止文件输出
            _ => Some(format!("{OUTPUT_DIR}/sols_{}", ext_instance.name)),
        }
    };

 let live_svg_path = if cfg!(feature = "live_svg") {
    match output_mode {
        OutputMode::StdoutOnly => None, //  不写文件
        _ => Some(format!("{LIVE_DIR}/.live_solution.svg")),
    }
} else {
    None
};

    let mut svg_exporter = SvgExporter::new(
        final_svg_path,
        intermediate_svg_dir,
        live_svg_path
    );
    if matches!(output_mode, OutputMode::StdoutOnly | OutputMode::Both) {
        svg_exporter = svg_exporter.with_emitter(|event| {
            use std::io::{self, Write};

            let mut stdout = io::stdout();
            let json = serde_json::to_string(&event).unwrap();

            writeln!(stdout, "{}", json).unwrap();
            stdout.flush().unwrap();
        });
    }
    /*.with_emitter(|event| {
        use std::io::{self, Write};

        let mut stdout = io::stdout();
        let json = serde_json::to_string(&event).unwrap();

        writeln!(stdout, "{}", json).unwrap();
        stdout.flush().unwrap();
    });*/
    
    // Create terminators for graceful shutdown
    let mut terminator = CombinedTerminator::new();
    info!("[MAIN] IPC control file: {}", terminator.ipc.control_file());
    info!("[MAIN] Working directory: {}", std::env::current_dir()?.display());

    let solution = optimize(
        instance.clone(),
        rng,
        &mut svg_exporter,
        &mut terminator,
        &config.expl_cfg,
        &config.cmpr_cfg,
        initial_solution.as_ref()
    );

    let json_path = format!("{OUTPUT_DIR}/final_{}.json", ext_instance.name);
    let json_output = ExtSPOutput {
        instance: ext_instance,
        solution: jagua_rs::probs::spp::io::export(&instance, &solution, *EPOCH)
    };

    // 1. First send FinalResult (data comes first)
    match output_mode {
        OutputMode::FileOnly | OutputMode::Both => {
            io::write_json(&json_output, Path::new(json_path.as_str()), Level::Info)?;
        }
        OutputMode::StdoutOnly => {
            let json_str = serde_json::to_string(&json_output)?;
            if let Some(emitter) = svg_exporter.emitter.as_ref() {
                emitter(SolverEvent::FinalResult {
                    data: general_purpose::STANDARD.encode(json_str),
                });
            }
        }
    }

    // 2. Then send GracefulShutdown status (if applicable)
    if let Some(reason) = terminator.shutdown_reason() {
        info!("Graceful shutdown triggered: {}, sending notification...", reason);
        if let Some(emitter) = svg_exporter.emitter.as_ref() {
            emitter(SolverEvent::GracefulShutdown {
                reason: reason.to_string(),
            });
        }
    }

    Ok(())
}
