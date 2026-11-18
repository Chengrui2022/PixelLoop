# 像素回环 PixelLoop

线性代数课程的信息隐藏实验，使用 Arnold 变换完成图像置乱和恢复。项目包含 MATLAB 实验代码，以及带加密、解密页面的 macOS 和 Windows 软件源码。

**Windows 10 / 11 x64** · [下载便携版 ZIP](downloads/PixelLoop-Windows-x64.zip?raw=true)。解压后双击 `PixelLoop.exe`。示例图已嵌入程序，运行只使用 Windows 自带的组件。

![实验示例图](matlab/arnold_sample.png)

## 项目结构

- `matlab/Arnold.m` 为 MATLAB 实验脚本，展示原图、置乱图和恢复图，并检查恢复结果。
- `matlab/arnold_sample.png` 为 128 × 128 的 RGB 示例图。
- `ui/` 为浏览器和 macOS 版使用的交互界面和算法。
- `mac/` 为使用 AppKit 和 WKWebView 的 macOS 应用入口。
- `windows/` 为 Windows 版的 Win32 图形界面、C 算法和交叉编译脚本。

## 运行 MATLAB 实验

在 MATLAB 中把当前文件夹切换到 `matlab`，运行 `Arnold.m`。

脚本采用课件矩阵，对示例图做一轮置乱和逆变换。运行后会并排显示三张图。命令行最后输出 `1`，表示恢复图与原图逐元素相等。

## 试用交互界面

直接用浏览器打开 `ui/index.html` 即可开始本地实验。

1. 使用内置示例图，或打开一张方形图片。
2. 设置参数 a、b 和轮数 k，运行加密。
3. 点击“转到解密”，带入置乱图和参数。
4. 运行解密，查看与原图逐像素比较的结果。

可以保存置乱 PNG，再在解密页面导入。导出文件名记录了 a、b 和 k，重新导入时需要填写这三个参数。

界面支持边长 2–1024 像素的方形 PNG 或 JPEG，输入图像需要没有透明像素。解密页面导入 PNG。图像处理在本地完成。

## 算法

本文约定 x 为行坐标、y 为列坐标，编号从 0 开始。

MATLAB 脚本使用课件矩阵。

$$
A=\begin{pmatrix}1&1\\1&2\end{pmatrix}
$$

交互软件使用带两个参数的广义 Arnold 矩阵。

$$
A(a,b)=\begin{pmatrix}1&a\\b&1+ab\end{pmatrix},
\qquad
A(a,b)^{-1}=\begin{pmatrix}1+ab&-a\\-b&1\end{pmatrix}
$$

把像素坐标写成列向量，左乘矩阵后分别模图像边长 N，得到目标坐标。像素值随坐标一起移动。a = b = 1 时与课件公式一致。

矩阵的行列式为 1，因此在模 N 下可逆。正向变换 k 轮后，可用逆矩阵变换 k 轮恢复。软件用矩阵快速幂计算 k 轮的合成坐标变换。

恢复原图需要使用对应的参数和轮数，并完整保留图像尺寸与像素值。无损 PNG 可以保存置乱结果。算法仅改变像素位置，保留颜色直方图，适用于课程实验和可逆置乱演示。

## 构建 macOS 版

需要 macOS 以及 Apple Command Line Tools。在仓库根目录执行以下命令，生成适用于 Apple 芯片和 Intel Mac 的应用。

```sh
mkdir -p "build/Arnold 图像置乱实验.app/Contents/MacOS"
mkdir -p "build/Arnold 图像置乱实验.app/Contents/Resources/ui"
cp mac/Info.plist "build/Arnold 图像置乱实验.app/Contents/Info.plist"
cp ui/* "build/Arnold 图像置乱实验.app/Contents/Resources/ui/"
xcrun swiftc -O -parse-as-library -target arm64-apple-macos12.0 mac/MacApp.swift -o build/ArnoldImageLab-arm64
xcrun swiftc -O -parse-as-library -target x86_64-apple-macos12.0 mac/MacApp.swift -o build/ArnoldImageLab-x86_64
xcrun lipo -create build/ArnoldImageLab-arm64 build/ArnoldImageLab-x86_64 -output "build/Arnold 图像置乱实验.app/Contents/MacOS/ArnoldImageLab"
codesign --force --sign - "build/Arnold 图像置乱实验.app"
```

## 从源码构建 Windows 版

在装有 Zig 0.15.2 和 `xxd` 的 macOS 或 Linux 上运行以下命令。生成的 `build/windows/PixelLoop.exe` 可独立复制到 Windows 10 或 11 x64 运行。

```sh
sh windows/build.sh
```

构建脚本把示例 PNG 嵌入可执行文件。界面调用 Win32，读写 PNG 和 JPEG 使用系统的 GDI+。

## 验证情况

- 经典 3 × 3 手算示例与实现结果一致。
- 使用不同尺寸、参数和轮数完成了 24 组全图往返测试。
- macOS 版已验证加密、参数错误提示、精确恢复、PNG 保存及重新导入。
- Windows 原生算法已通过 24 组图像往返测试。已检查便携包的 x64 图形程序格式和系统 DLL 依赖，Windows 实机运行待验证。

## 制作说明与参考资料

MATLAB 脚本由学生在逐步学习中编写，交互软件借助 AI 制作。示例图为 AI 生成图片。

- 课程课件《矩阵乘法运算的应用》中的图像 Arnold 置乱公式。
- [MATLAB 官方文档](https://www.mathworks.com/help/matlab/)
- [Microsoft GDI+ 文档](https://learn.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-gdi-start)
- [Apple WKWebView 文档](https://developer.apple.com/documentation/webkit/wkwebview)
