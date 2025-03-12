#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
GAPBS和其他工作负载自动化运行脚本
运行所有程序并将日志保存到artifact文件夹的相应位置
"""

import os
import subprocess
import time
import argparse
import logging
import shutil
from pathlib import Path
from datetime import datetime

# 配置日志格式
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

# 定义基本路径
ARTIFACT_BASE = "artifact"
CXL_MEM_SIM = "./CXLMemSim"

# 定义工作负载配置
WORKLOADS = {
    "gapbs": {
        "path": "gapbs",
        "programs": [
            "bc", "bfs", "cc", "pr", "sssp", "tc"  # GAPBS提供的所有算法
        ],
        "args": "-g 16 -n 1",  # 默认参数
        "env": {}  # 默认环境变量
    },
    "memcached": {
        "path": "memcached",
        "programs": ["memcached"],
        "args": "-m 1024 -t 4 -p 11211",
        "env": {}
    },
    "llama": {
        "path": "llama",
        "programs": ["llama"],
        "args": "--model 7B --n-predict 100 --prompt \"Once upon a time\"",
        "env": {}
    },
    "gromacs": {
        "path": "gromacs",
        "programs": ["gmx"],
        "args": "mdrun -s topol.tpr -nsteps 1000",
        "env": {}
    },
    "vsag": {
        "path": "vsag",
        "programs": ["vsag"],
        "args": "",
        "env": {}
    },
    "microbench": {
        "path": "microbench",
        "programs": ["ld", "st", "ld_serial", "st_serial", "malloc", "writeback"],
        "args": "",
        "env": {}
    }
}

def ensure_directory(path):
    """确保目录存在，如果不存在则创建"""
    os.makedirs(path, exist_ok=True)
    return path

def run_command(cmd, log_path=None, env=None, timeout=3600):
    """
    运行命令并捕获输出

    参数:
        cmd (list): 命令列表
        log_path (str, optional): 日志保存路径
        env (dict, optional): 环境变量
        timeout (int, optional): 超时时间（秒）

    返回:
        tuple: (返回码, 输出)
    """
    logger.info(f"运行命令: {' '.join(cmd)}")

    # 准备环境变量
    run_env = os.environ.copy()
    if env:
        run_env.update(env)

    try:
        # 运行命令
        process = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=run_env,
            text=True,
            timeout=timeout,
            check=False
        )

        # 记录输出
        output = process.stdout
        if log_path:
            with open(log_path, 'w') as f:
                f.write(output)
            logger.info(f"输出已保存到: {log_path}")

        return process.returncode, output

    except subprocess.TimeoutExpired:
        logger.error(f"命令执行超时（{timeout}秒）: {' '.join(cmd)}")
        if log_path:
            with open(log_path, 'w') as f:
                f.write(f"TIMEOUT: Command timed out after {timeout} seconds\n")
        return -1, f"TIMEOUT: Command timed out after {timeout} seconds"

    except Exception as e:
        logger.error(f"命令执行失败: {e}")
        if log_path:
            with open(log_path, 'w') as f:
                f.write(f"ERROR: {str(e)}\n")
        return -2, f"ERROR: {str(e)}"

def run_original(workload, program, args, base_dir):
    """运行原始程序"""
    program_path = os.path.join(WORKLOADS[workload]["path"], program)
    log_path = os.path.join(base_dir, "orig.txt")

    # 构建命令
    cmd = [program_path]
    if args:
        cmd.extend(args.split())

    # 运行命令
    return run_command(cmd, log_path, WORKLOADS[workload].get("env"))

def run_cxl_mem_sim(workload, program, args, base_dir, pebs_period=10, latency="200,250,200,250,200,250", bandwidth="50,50,50,50,50,50"):
    """使用CXLMemSim运行程序"""
    program_path = os.path.join(WORKLOADS[workload]["path"], program)
    log_path = os.path.join(base_dir, "cxlmemsim.txt")

    # 构建命令
    cmd = [
        CXL_MEM_SIM,
        "-t", program_path,
        "-p", str(pebs_period),
        "-l", latency,
        "-b", bandwidth
    ]

    # 添加程序参数（如果有）
    if args:
        cmd[3] = f"{program_path} {args}"

    # 运行命令
    return run_command(cmd, log_path, WORKLOADS[workload].get("env"))

def run_all_workloads(args):
    """运行所有工作负载"""
    start_time = time.time()
    logger.info("开始运行所有工作负载")

    # 创建artifact总目录
    ensure_directory(ARTIFACT_BASE)

    # 记录系统信息
    if args.collect_system_info:
        logger.info("收集系统信息")
        run_command(["dmesg"], os.path.join(ARTIFACT_BASE, "dmesg.txt"))
        run_command(["dmidecode"], os.path.join(ARTIFACT_BASE, "dmidecode.txt"))
        run_command(["lspci", "-vvv"], os.path.join(ARTIFACT_BASE, "lspci.txt"))

    # 遍历所有工作负载
    for workload_name, workload_config in WORKLOADS.items():
        if args.workloads and workload_name not in args.workloads:
            logger.info(f"跳过工作负载: {workload_name}")
            continue

        logger.info(f"开始处理工作负载: {workload_name}")

        # 遍历工作负载中的所有程序
        for program in workload_config["programs"]:
            if args.programs and program not in args.programs:
                logger.info(f"跳过程序: {program}")
                continue

            logger.info(f"开始处理程序: {program}")

            # 创建程序的目录
            program_dir = ensure_directory(os.path.join(ARTIFACT_BASE, workload_name, program))

            # 运行原始程序（如果需要）
            if args.run_original:
                logger.info(f"运行原始程序: {program}")
                returncode, _ = run_original(
                    workload_name,
                    program,
                    workload_config["args"],
                    program_dir
                )
                if returncode != 0 and not args.ignore_errors:
                    logger.error(f"原始程序运行失败: {program}, 返回码: {returncode}")
                    if args.stop_on_error:
                        return

            # 运行CXLMemSim（如果需要）
            if args.run_cxlmemsim:
                logger.info(f"使用CXLMemSim运行程序: {program}")
                returncode, _ = run_cxl_mem_sim(
                    workload_name,
                    program,
                    workload_config["args"],
                    program_dir,
                    args.pebs_period,
                    args.latency,
                    args.bandwidth
                )
                if returncode != 0 and not args.ignore_errors:
                    logger.error(f"CXLMemSim运行失败: {program}, 返回码: {returncode}")
                    if args.stop_on_error:
                        return

    # 完成所有工作负载
    end_time = time.time()
    elapsed_time = end_time - start_time
    hours, remainder = divmod(elapsed_time, 3600)
    minutes, seconds = divmod(remainder, 60)

    logger.info(f"所有工作负载运行完成，总耗时: {int(hours)}时{int(minutes)}分{int(seconds)}秒")

    # 将运行日志保存到artifact目录
    if args.log_file:
        shutil.copy2(args.log_file, os.path.join(ARTIFACT_BASE, "run.log"))

def main():
    parser = argparse.ArgumentParser(description="GAPBS和其他工作负载自动化运行脚本")
    parser.add_argument("--workloads", nargs="+", help="要运行的工作负载，默认运行所有")
    parser.add_argument("--programs", nargs="+", help="要运行的程序，默认运行所有")
    parser.add_argument("--run-original", action="store_true", help="运行原始程序")
    parser.add_argument("--run-cxlmemsim", action="store_true", help="使用CXLMemSim运行程序")
    parser.add_argument("--collect-system-info", action="store_true", help="收集系统信息")
    parser.add_argument("--pebs-period", type=int, default=10, help="PEBS采样周期")
    parser.add_argument("--latency", default="200,250,200,250,200,250", help="CXLMemSim延迟设置")
    parser.add_argument("--bandwidth", default="50,50,50,50,50,50", help="CXLMemSim带宽设置")
    parser.add_argument("--ignore-errors", action="store_true", help="忽略错误继续运行")
    parser.add_argument("--stop-on-error", action="store_true", help="遇到错误时停止运行")
    parser.add_argument("--log-file", default="run.log", help="运行日志文件")
    args = parser.parse_args()

    # 设置日志文件
    file_handler = logging.FileHandler(args.log_file)
    file_handler.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))
    logger.addHandler(file_handler)

    # 如果没有指定运行类型，默认都运行
    if not args.run_original and not args.run_cxlmemsim:
        args.run_original = True
        args.run_cxlmemsim = True

    # 运行所有工作负载
    run_all_workloads(args)

if __name__ == "__main__":
    main()