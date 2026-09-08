[English](README.md)

# 无线生态观察器

ESP32-C3 / 240 × 320 / ESP-IDF 5.5.3 独立固件。参考 https://github.com/folotoy/ai-passport
的 Wi-Fi STA 扫描流程，复用显示与 ADC 按键 BSP，基准提交
`df3990726e3751fadaaaa703a480dbba6e13c61b`，许可证见 `components/bsp/LICENSE`。

## 当前交互

界面名称为「无线小森林」，使用中文短句。去除信道数字、AP 计数、dBm 坐标与 PEAK 数值、刷新序号和倒计时。
主画面以分层像素松树展示环境，树越高表示信号越强。已移除解释树高的提示行，将原来的 32 像素提示区全部并入树林，展示区高度由 136 增至 168 像素，树木高度同步放大。信道仍在内部决定横向顺序及树木颜色，屏幕无需阅读技术指标。
树下箭头指出当前选择，底部显示「这棵树的信号很强／适中／较弱」；短暂漏扫显示「信号正在消失」。
信号强度下方显示选中树木对应的 Wi-Fi 名称，使用黑色文字；绿色环境摘要及「风景会自动变化」已移除。
没有热点时只显示一条提示，名称行留空。名称取自该信道最强平滑信号的热点，上下选树时同步切换。
支持英文、数字和基本汉字，超长名称自动横向滚动，隐藏名称显示「隐藏网络」；未覆盖的字符或无效编码显示问号。
演示模式始终标注「演示风景」，其中的名称为人工示例。观察器只扫描，显示名称不表示已连接该网络。

- 开机自动观察真实 2.4 GHz 环境，每 5 秒扫描一次，不重叠。失败保留前一帧并自动重试。
- 上下键循环选择已有的树，跳过没有树的位置。选中的树消失后自动选择另一棵。
- 长按 DOWN 约 2 秒切换真实和演示模式；演示每 5 秒自动循环三个环境。
- OK 在真实模式按扫描间隔请求更新，在演示模式立即切换下一组环境。
- 无盖章、命名或收藏功能，无互联网或 AI 服务依赖。

每个信道的一棵树代表该信道中保留热点的最强平滑信号。最多追踪 24 个热点，RSSI 使用 3:1 平滑，
连续两次成功扫描未发现后移除。强／适中／弱的内部阈值为 -55 / -75 dBm。
这些细节保留在文档中，界面只呈现自然语言。较高扫描频率的耗电与移动体验仍需真机验证。

## 模拟器测试与固件

根目录执行 `npm start`，打开 http://127.0.0.1:4190，加载
`public/assets/firmware/wireless-ecology.bin`。这是地址 0x0 的 Full Flash 合并镜像。
模拟器只提供虚拟 Wi-Fi，不能扫描电脑周围热点。长按 DOWN 看演示森林自动变化，上下选择树木阅读信号描述。

## 编译与校验

从根目录运行，Docker 可替代 Podman：

```powershell
podman run --rm -v "${PWD}:/project" -w /project/firmware/wireless-ecology docker.io/espressif/idf:v5.5.3 idf.py build merge-bin
node tools/package-ecology.mjs
gcc -std=c11 -Wall -Wextra -Werror firmware/wireless-ecology/test_model.c -o .toolchains/ecology-test.exe
& .toolchains/ecology-test.exe
```

激活本机 IDF 后也可进入固件目录执行 `idf.py build merge-bin`。打包结果及 SHA-256 清单在
`artifacts/wireless-ecology/`。保留上游 3 MiB 应用上限、cardid 0x356000、Recovery 0x700000；
不补齐到 8 MiB、不擦除旧 NVS、不写工厂分区。

显示使用 38,400 字节帧缓冲及 4,800 字节 DMA 条带。扫描工作任务与 UI 通过队列通信，
按键回调不执行扫描或绘制。主机测试覆盖平滑、漏扫、信道汇总、最强信号、横轴位置与高度边界。
启动 4193 端口的服务后执行 `node tools/ecology-firmware-smoke.mjs` 可验证中文文字、
自动演示及树木选择，需 `.toolchains/browser` 下的 Playwright 和本机 Chrome；地址可由 `ECO_TEST_URL` 指定。
真机扫描覆盖率、移动体验、屏幕可读性及 5 秒扫描间隔的耗电仍需实物验证。

中文标签已嵌入固件，不需要运行时字体服务。修改文案后可执行 `python tools/generate-ecology-labels.py` 重新生成，需 Pillow 和 Windows 微软雅黑字体。

SSID 字库为 `main/name_font.bin`，已嵌入应用；执行 `python tools/generate-ecology-name-font.py` 可重建，需要 Pillow 和微软雅黑。主机测试覆盖最强热点名称对应、名称更新及 UTF-8 解码，模拟器检查覆盖中英文名称和选树切换。
