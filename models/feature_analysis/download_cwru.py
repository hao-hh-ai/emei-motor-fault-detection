#!/usr/bin/env python3
"""
CWRU 数据集下载器
==================
从镜像源下载 CWRU 轴承数据集压缩包，解压到 models/cwru_data/

用法: python download_cwru.py
"""

import os
import sys
import zipfile
import urllib.request

# Community mirrors of CWRU dataset (tried in order)
CWRU_MIRRORS = [
    "https://github.com/AiChiXiaoXiongBingGan/CWRU-dataset/archive/refs/heads/main.zip",
    "https://github.com/s-whynot/cwru-bearing-dataset/archive/refs/heads/master.zip",
]

# Alternative: original CWRU data files
# Visit https://engineering.case.edu/bearingdatacenter/download-data-file
# and download the "12k Drive End Bearing Fault Data" archive manually.


def download_with_progress(url, dest):
    """下载文件并显示进度"""
    print(f"下载: {url}")
    print(f"目标: {dest}")

    def hook(count, block_size, total_size):
        if total_size > 0:
            pct = min(count * block_size * 100 / total_size, 100)
            downloaded = min(count * block_size, total_size)
            sys.stdout.write(
                f"\r  {downloaded / 1024 / 1024:.1f} / "
                f"{total_size / 1024 / 1024:.1f} MB ({pct:.0f}%)"
            )
            sys.stdout.flush()

    try:
        urllib.request.urlretrieve(url, dest, reporthook=hook)
        print("\n下载完成!")
        return True
    except Exception as e:
        print(f"\n下载失败: {e}")
        return False


def main():
    data_dir = os.path.join(os.path.dirname(__file__), "..", "cwru_data")
    os.makedirs(data_dir, exist_ok=True)

    zip_path = os.path.join(data_dir, "cwru_mirror.zip")
    success = False

    for url in CWRU_MIRRORS:
        if download_with_progress(url, zip_path):
            success = True
            break
        print(f"  镜像失败, 尝试下一个...")

    if success:
        print("解压中...")
        try:
            with zipfile.ZipFile(zip_path, 'r') as zf:
                # 从 GitHub archive zip 提取 .mat 文件 (可能嵌套在子目录中)
                for member in zf.namelist():
                    if member.endswith('.mat'):
                        target = os.path.join(data_dir, os.path.basename(member))
                        with zf.open(member) as src, open(target, 'wb') as dst:
                            dst.write(src.read())
                print(f"解压完成 → {data_dir} ({len([f for f in os.listdir(data_dir) if f.endswith('.mat')])} .mat 文件)")
            # 清理 zip
            os.remove(zip_path)
            print("清理完成.\n")
            print("现在可以运行: python extract_features.py")
        except zipfile.BadZipFile:
            print("[ERROR] 下载的 ZIP 文件损坏, 请重试.")
            print_manual()
    else:
        print_manual()

def print_manual():
    print("\n自动下载失败. 手动下载说明:")
    print("  1. 打开浏览器访问:")
    print("     https://engineering.case.edu/bearingdatacenter/download-data-file")
    print("  2. 下载 Normal Baseline Data + 12k Drive End Bearing Fault Data")
    print("  3. 将所有 *.mat 文件解压到: models/cwru_data/")


if __name__ == '__main__':
    main()
