#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
时间戳差异分析脚本 - 修复版本
分析输入文件中相邻时间戳之间的差异，并生成图表
"""

import re
import subprocess
import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import time
import argparse
from pathlib import Path

def run_test(target, pebs_period, output_dir, run_id=None):
    """
    使用指定的PEBS周期运行目标程序

    参数:
        target (str): 目标程序路径
        pebs_period (int): PEBS采样周期
        output_dir (str): 输出目录
        run_id (str, optional): 运行ID，用于区分不同的运行

    返回:
        str或None: 程序输出或None（如果失败）
    """
    timestamp = time.strftime("%Y%m%d-%H%M%S")
    run_id = run_id or timestamp

    # 确保输出目录存在
    os.makedirs(output_dir, exist_ok=True)

    # 创建输出文件路径
    output_file = os.path.join(output_dir, f"{os.path.basename(target)}_pebs{pebs_period}_{run_id}.log")

    # 构建命令
    cmd = [
        "./CXLMemSim",
        "-t", target,
        "-p", str(pebs_period),
        "-c", "0,1,2,3",  # 使用前4个CPU核
        "-d", "110",      # DRAM延迟
        "-m", "p",        # 页面模式
    ]

    # 运行命令并捕获输出
    try:
        print(f"执行命令: {' '.join(cmd)}")
        with open(output_file, 'w') as f:
            result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, check=True, timeout=300)  # 5分钟超时
            f.write(result.stdout)
        return result.stdout

    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as e:
        print(f"命令执行失败: {e}")
        with open(output_file, 'w') as f:
            f.write(f"错误: {e}\n")
            if hasattr(e, 'stdout') and e.stdout:
                f.write(e.stdout)
        return None

def parse_timestamps(content):
    """
    从程序输出文本中解析时间戳和索引

    参数:
        content (str): 程序输出内容

    返回:
        pandas.DataFrame: 包含时间戳和索引的DataFrame，如果没有有效数据则为None
    """
    timestamps = []
    indices = []

    if not content:
        return None

    for line in content.split('\n'):
        # 使用正则表达式匹配时间戳和索引
        match = re.match(r'^\s*(\d+)\s+(\d+)\s*$', line)
        if match:
            timestamp = int(match.group(1))
            index = int(match.group(2))
            timestamps.append(timestamp)
            indices.append(index)

    if not timestamps:  # 如果没有找到有效数据
        return None

    # 创建DataFrame
    df = pd.DataFrame({'timestamp': timestamps, 'index': indices})
    return df

def calculate_time_diffs(df):
    """
    计算相邻时间戳之间的差异

    参数:
        df (pandas.DataFrame): 包含时间戳和索引的DataFrame

    返回:
        pandas.DataFrame: 包含时间戳差异的DataFrame，如果输入为None则返回None
    """
    if df is None or df.empty:
        return None

    # 计算时间戳差异（纳秒）
    df['time_diff'] = df['timestamp'].diff()

    # 第一行的差异为NaN，将其删除
    df = df.dropna()

    if df.empty:
        return None

    # 将纳秒转换为微秒，便于可视化
    df['time_diff_us'] = df['time_diff'] / 1000

    return df

def plot_time_diffs(df, output_file=None):
    """
    绘制时间戳差异图表

    参数:
        df (pandas.DataFrame): 包含时间戳差异的DataFrame
        output_file (str, optional): 输出文件路径
    """
    if df is None or df.empty:
        print("没有数据可以绘制图表")
        return

    plt.figure(figsize=(12, 8))

    # 绘制时间差异随索引变化的散点图
    plt.subplot(2, 1, 1)
    plt.scatter(df['index'], df['time_diff_us'], alpha=0.6, s=5)
    plt.yscale('log')  # 使用对数刻度以便更好地查看不同数量级的差异
    plt.xlabel('Index')
    plt.ylabel('Time Difference (us)')
    plt.title('Time Difference vs Index')
    plt.grid(True, which="both", ls="--", alpha=0.5)

    # 绘制时间差异的直方图
    plt.subplot(2, 1, 2)
    plt.hist(df['time_diff_us'], bins=100, alpha=0.7)
    plt.xscale('log')  # 使用对数刻度
    plt.xlabel('Time Difference (us)')
    plt.ylabel('Frequency')
    plt.title('Time Difference Distribution')
    plt.grid(True, which="both", ls="--", alpha=0.5)

    plt.tight_layout()

    # 保存图表或显示
    if output_file:
        plt.savefig(output_file)
        print(f"图表已保存到: {output_file}")
    else:
        plt.show()

def analyze_patterns(df):
    """
    分析时间戳差异的模式和统计信息

    参数:
        df (pandas.DataFrame): 包含时间戳差异的DataFrame

    返回:
        dict: 包含分析结果的字典，如果输入为None则返回None
    """
    if df is None or df.empty:
        return None

    results = {}

    # 基本统计信息
    results['min_diff'] = df['time_diff'].min()
    results['max_diff'] = df['time_diff'].max()
    results['mean_diff'] = df['time_diff'].mean()
    results['median_diff'] = df['time_diff'].median()
    results['std_diff'] = df['time_diff'].std()

    # 寻找主要的时间差异模式 - 使用简单的聚类方法来发现主要的时间差异组
    diff_clusters = {}
    threshold = max(results['std_diff'] / 10, 1)  # 使用标准差的1/10作为阈值，但至少为1纳秒

    for diff in df['time_diff']:
        # 查找是否存在接近的簇
        found_cluster = False
        for center in list(diff_clusters.keys()):
            if abs(diff - center) < threshold:
                diff_clusters[center] += 1
                found_cluster = True
                break

        # 如果没有找到接近的簇，创建新的簇
        if not found_cluster:
            diff_clusters[diff] = 1

    # 按照频率排序，找出主要的时间差异模式
    sorted_clusters = sorted(diff_clusters.items(), key=lambda x: x[1], reverse=True)
    results['main_patterns'] = sorted_clusters[:10]  # 取前10个主要模式

    return results

def create_pattern_analysis(df, pattern_file='timestamp_patterns.pdf'):
    """
    创建时间差异模式分析图

    参数:
        df (pandas.DataFrame): 包含时间戳差异的DataFrame
        pattern_file (str): 输出文件名
    """
    if df is None or df.empty:
        print("没有数据可以绘制模式分析图")
        return

    plt.figure(figsize=(14, 10))

    # 绘制时间差异随时间的变化
    plt.subplot(3, 1, 1)
    plt.plot(df['timestamp'], df['time_diff_us'], '-o', markersize=2, alpha=0.6)
    plt.xlabel('Timestamp')
    plt.ylabel('Time Difference (us)')
    plt.title('Time Difference over Time')
    plt.grid(True)

    # 绘制移动平均值
    plt.subplot(3, 1, 2)
    window_size = max(5, min(50, len(df) // 10))  # 使用数据长度的1/10作为窗口大小，但至少为5，最大为50
    df['rolling_mean'] = df['time_diff_us'].rolling(window=window_size, min_periods=1).mean()
    plt.plot(df['index'], df['rolling_mean'], 'r-')
    plt.xlabel('Index')
    plt.ylabel('Moving Average of Time Diff (us)')
    plt.title(f'Moving Average of Time Differences ({window_size} points)')
    plt.grid(True)

    # 绘制相邻差异之间的关系，检查是否存在周期性
    plt.subplot(3, 1, 3)
    if len(df) > 1:  # 确保有足够的数据点
        plt.scatter(df['time_diff_us'].iloc[:-1].values,
                    df['time_diff_us'].iloc[1:].values,
                    alpha=0.5, s=5)
        plt.xlabel('Current Time Difference (us)')
        plt.ylabel('Next Time Difference (us)')
        plt.title('Relationship between Adjacent Time Differences')
        plt.grid(True)
    else:
        plt.text(0.5, 0.5, 'Not enough data points for analysis',
                 horizontalalignment='center', verticalalignment='center')

    plt.tight_layout()
    plt.savefig(pattern_file)
    print(f"模式分析图表已保存到: {pattern_file}")

def create_interval_analysis(df, intervals_file='timestamp_intervals.pdf'):
    """
    创建时间间隔分组分析图

    参数:
        df (pandas.DataFrame): 原始时间戳数据
        intervals_file (str): 输出文件名
    """
    if df is None or df.empty or len(df) < 2:
        print("没有足够的数据可以绘制间隔分析图")
        return

    # 创建时间间隔分组分析
    intervals = []
    interval_indices = []
    current_interval_start = df['timestamp'].iloc[0]
    current_interval_idx = df['index'].iloc[0]

    for i in range(1, len(df)):
        if df['timestamp'].iloc[i] - df['timestamp'].iloc[i-1] > 5000000:  # 超过5毫秒认为是新的间隔
            interval_length = df['timestamp'].iloc[i-1] - current_interval_start
            intervals.append(interval_length)
            interval_indices.append((current_interval_idx, df['index'].iloc[i-1]))
            current_interval_start = df['timestamp'].iloc[i]
            current_interval_idx = df['index'].iloc[i]

    # 添加最后一个间隔
    interval_length = df['timestamp'].iloc[-1] - current_interval_start
    intervals.append(interval_length)
    interval_indices.append((current_interval_idx, df['index'].iloc[-1]))

    if not intervals:  # 如果没有识别到有效间隔
        print("未识别到有效时间间隔")
        return

    plt.figure(figsize=(12, 6))
    plt.bar(range(len(intervals)), [i/1000000 for i in intervals])
    plt.xlabel('Interval Number')
    plt.ylabel('Interval Duration (ms)')
    plt.title('Time Interval Distribution')
    plt.grid(True, axis='y')

    # 添加索引范围标签
    for i, (start_idx, end_idx) in enumerate(interval_indices):
        plt.text(i, intervals[i]/1000000 + 0.5, f"{start_idx}-{end_idx}",
                 horizontalalignment='center', rotation=90)

    plt.tight_layout()
    plt.savefig(intervals_file)
    print(f"间隔分析图表已保存到: {intervals_file}")

def main():
    parser = argparse.ArgumentParser(description='分析时间戳差异并生成图表')
    parser.add_argument('--output', '-o', help='输出图表文件路径')
    parser.add_argument('--detailed', '-d', action='store_true', help='输出详细的分析结果')
    parser.add_argument('--targets', nargs='+', default=['./microbench/ld1'],
                        help='要测试的目标程序列表')
    parser.add_argument('--pebs-periods', nargs='+', type=int, default=[1, 10, 100, 1000, 10000],
                        help='要测试的PEBS周期列表')
    parser.add_argument('--output-dir', default='./results',
                        help='结果输出目录')
    parser.add_argument('--runs', type=int, default=1,
                        help='每个组合运行的次数')
    parser.add_argument('--skip-test', action='store_true', help='跳过测试运行，仅处理已有的日志文件')
    parser.add_argument('--log-file', help='直接处理单个日志文件，而不运行测试')
    args = parser.parse_args()

    successful_df = None

    # 如果提供了日志文件，直接处理它
    if args.log_file:
        print(f"直接处理日志文件: {args.log_file}")
        try:
            with open(args.log_file, 'r') as f:
                content = f.read()
            df = parse_timestamps(content)
            if df is not None and not df.empty:
                successful_df = df
                print(f"成功从日志文件中提取了 {len(df)} 条时间戳记录")
            else:
                print("无法从日志文件中提取有效的时间戳数据")
        except Exception as e:
            print(f"处理日志文件时出错: {e}")

    # 否则运行测试并处理结果
    elif not args.skip_test:
        # 为每个目标程序和PEBS周期的组合运行测试
        for target in args.targets:
            for pebs_period in args.pebs_periods:
                for run in range(args.runs):
                    print(f"\n运行目标: {target}, PEBS周期: {pebs_period}, 运行 #{run+1}/{args.runs}")
                    run_id = f"run{run+1}"

                    output = run_test(target, pebs_period, args.output_dir, run_id)
                    if output:
                        df = parse_timestamps(output)
                        if df is not None and not df.empty:
                            print(f"成功提取了 {len(df)} 条时间戳记录")
                            successful_df = df
                            # 找到有效数据后可以提前停止
                            break

                # 如果找到了有效数据，就停止测试
                if successful_df is not None:
                    break

            # 如果找到了有效数据，就停止测试
            if successful_df is not None:
                break

    # 处理数据并生成图表
    if successful_df is not None:
        # 计算时间差异
        df_with_diffs = calculate_time_diffs(successful_df)

        if df_with_diffs is not None and not df_with_diffs.empty:
            # 保存原始数据到CSV文件
            csv_file = os.path.join(args.output_dir, 'timestamp_data.csv')
            successful_df.to_csv(csv_file, index=False)
            print(f"原始时间戳数据已保存到: {csv_file}")

            # 保存处理后的数据到CSV文件
            diffs_csv_file = os.path.join(args.output_dir, 'timestamp_diffs.csv')
            df_with_diffs.to_csv(diffs_csv_file, index=False)
            print(f"时间戳差异数据已保存到: {diffs_csv_file}")

            # 绘制时间差异图表
            output_file = args.output if args.output else os.path.join(args.output_dir, 'timestamp_diffs.pdf')
            plot_time_diffs(df_with_diffs, output_file)

            # 创建模式分析图表
            pattern_file = os.path.join(args.output_dir, 'timestamp_patterns.pdf')
            create_pattern_analysis(df_with_diffs, pattern_file)

            # 创建间隔分析图表
            intervals_file = os.path.join(args.output_dir, 'timestamp_intervals.pdf')
            create_interval_analysis(successful_df, intervals_file)

            # 输出详细分析结果
            if args.detailed:
                results = analyze_patterns(df_with_diffs)
                if results:
                    print("\n时间戳差异分析结果:")
                    print(f"最小差异: {results['min_diff']} ns ({results['min_diff']/1000:.2f} μs)")
                    print(f"最大差异: {results['max_diff']} ns ({results['max_diff']/1000:.2f} μs)")
                    print(f"平均差异: {results['mean_diff']:.2f} ns ({results['mean_diff']/1000:.2f} μs)")
                    print(f"中位差异: {results['median_diff']} ns ({results['median_diff']/1000:.2f} μs)")
                    print(f"标准差: {results['std_diff']:.2f} ns ({results['std_diff']/1000:.2f} μs)")

                    print("\n主要时间差异模式:")
                    for i, (diff, count) in enumerate(results['main_patterns']):
                        print(f"{i+1}. {diff:.2f} ns ({diff/1000:.2f} μs) - 出现 {count} 次")

                    # 保存统计结果到文件
                    stats_file = os.path.join(args.output_dir, 'timestamp_stats.txt')
                    with open(stats_file, 'w') as f:
                        f.write("时间戳差异分析统计信息\n")
                        f.write("======================\n\n")
                        f.write(f"数据点数: {len(successful_df)}\n")
                        f.write(f"总时间: {(successful_df['timestamp'].iloc[-1] - successful_df['timestamp'].iloc[0])/1000000000:.6f} 秒\n")
                        f.write(f"平均采样率: {len(successful_df) / ((successful_df['timestamp'].iloc[-1] - successful_df['timestamp'].iloc[0]) / 1000000000):.2f} 点/秒\n\n")

                        f.write("时间差异统计:\n")
                        f.write(f"  最小差异: {results['min_diff']} 纳秒 ({results['min_diff']/1000:.2f} 微秒)\n")
                        f.write(f"  最大差异: {results['max_diff']} 纳秒 ({results['max_diff']/1000:.2f} 微秒)\n")
                        f.write(f"  平均差异: {results['mean_diff']:.2f} 纳秒 ({results['mean_diff']/1000:.2f} 微秒)\n")
                        f.write(f"  中位差异: {results['median_diff']} 纳秒 ({results['median_diff']/1000:.2f} 微秒)\n")
                        f.write(f"  标准差: {results['std_diff']:.2f} 纳秒 ({results['std_diff']/1000:.2f} 微秒)\n\n")

                    print(f"统计信息已保存到: {stats_file}")
        else:
            print("无法计算时间差异，可能是数据点不足")
    else:
        print("没有找到有效的时间戳数据。请检查目标程序的输出或提供有效的日志文件。")

if __name__ == "__main__":
    main()