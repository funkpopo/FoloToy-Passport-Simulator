[English](README.md)

# 无线生态观察器

ESP32-C3 / 240 × 320 / ESP-IDF 5.5.3 独立固件，落实根目录 ADVICES.md 的第 4 个玩法。
参考并复用 https://github.com/folotoy/ai-passport 的 Wi-Fi STA 扫描流程、显示与 ADC 按键 BSP，
上游提交 `df3990726e3751fadaaaa703a480dbba6e13c61b`，许可证见 `components/bsp/LICENSE`。
使用 RGB565 像素画，不依赖互联网、AI、LVGL 或图片资源服务。

## 模拟器手动测试

在仓库根目录执行 `npm start`，打开 http://127.0.0.1:4190，使用加载固件入口选择：

`public/assets/firmware/wireless-ecology.bin`

这是从地址 0x0 加载的 Full Flash 合并镜像。不要选择 build 下仅包含应用的 bin。
开机默认 `LIVE / 2.4 GHZ`，模拟器提供的是虚拟 Wi-Fi 环境，不能扫描电脑周围真实热点。
**长按 DOWN 约 2 秒进入 `DEMO / SYNTHETIC`**，即可查看丰富的像素景观并测试环境变化。
屏幕采用英文短标签，完整操作如下：

| 场景 | 按键 | 行为 |
| --- | --- | --- |
| 景观 | UP / ↑ | 打开收藏册 |
| 真实景观 | DOWN / ↓ | 重新扫描，最短间隔 5 秒 |
| 演示景观 | DOWN / ↓ | 循环切换 3 组人工热点数据 |
| 景观 | 长按 DOWN / ↓ | 切换真实扫描与演示数据 |
| 景观 | OK / Enter | 冻结当前景观，打开命名选择 |
| 命名 | UP、DOWN | 循环选择 8 个预设名称 |
| 命名 | OK | 盖章保存到 Flash |
| 命名 | 长按 OK | 取消 |
| 收藏册 | DOWN | 下一张收藏 |
| 收藏册 | OK | 修改当前收藏名称 |
| 收藏册 | UP | 返回景观 |

建议测试顺序：开机 → 长按 DOWN → 短按 DOWN 切换环境 → OK 选名并保存 → UP 查看收藏 →
DOWN 翻页 → OK 改名 → UP 返回 → 长按 DOWN 回到真实扫描。
收藏最多 6 张，第 7 次保存覆盖最早的槽位；收藏包括地形、树木状态、名称和来源标识。
真机收藏保存在 NVS；模拟器重启是否保留收藏取决于模拟器是否保留已写入的 Flash。
重新上传原始 bin 会恢复空白收藏数据。

## 图像规则与边界

- 每个热点用 BSSID 的 32 位散列固定树木位置、树形和生物位置，不依赖扫描返回顺序。
- RSSI 越强，树木越高，范围 8–38 个逻辑像素。后续扫描使用 3:1 平滑。
- 信道 1–4 对应绿色、5–9 对应琥珀色、10–14 对应蓝色地形，屏幕底部有图例。
- 一个热点伴随一只像素生物；首次漏扫变灰，连续两次成功扫描未出现后移除。
  扫描失败保留旧景观。`+ / -` 表示最近一次更新新增、移除的树木。
- 显示扫描热点总数，最多绘制 24 棵树；保留一轮漏扫的树木也占名额。
- 真实模式每 30 秒被动扫描一次；演示、命名与收藏界面暂停自动扫描。
  已经开始的扫描会完成，演示和真实景观状态互相独立。
- RSSI 与信道仅用于艺术映射，不能测量信道拥堵或推断附近人数；ESP32-C3 仅支持 2.4 GHz。
- 当前命名为 8 个英文预设选项，尚无自由文本输入。耗电、实际扫描覆盖率及移动体验需真机验证。

## 构建与验证

从根目录运行（Docker 可替代 Podman）：

```powershell
podman run --rm -v "${PWD}:/project" -w /project/firmware/wireless-ecology docker.io/espressif/idf:v5.5.3 idf.py build merge-bin
node tools/package-ecology.mjs
gcc -std=c11 -Wall -Wextra -Werror firmware/wireless-ecology/test_model.c -o .toolchains/ecology-test.exe
& .toolchains/ecology-test.exe
```

激活本地 ESP-IDF 5.5.3 后，也可进入本目录执行 `idf.py build merge-bin`。
打包脚本验证分区 MD5、镜像内容和受保护分区地址，输出固件、分离镜像及 SHA-256 清单到
`artifacts/wireless-ecology/`，并把可加载镜像复制到 `public/assets/firmware/`。
沿用上游 3 MiB 应用上限、`cardid` 的 0x356000 地址和 Recovery 的 0x700000 地址。
镜像不填充到 8 MiB、不写入工厂分区；NVS 初始化失败时不会自动擦除原有数据。

应用内存包括 38,400 字节逻辑帧缓冲与 4,800 字节 DMA 条带，等待传输完成后才重用条带。
按键回调只发送队列，网络扫描运行在工作任务，绘制和 NVS 写入由主任务串行处理。
主机测试覆盖热点身份稳定、RSSI 平滑、漏扫恢复、消失、数量上限和信号映射边界。

可选模拟器检查：启动 4193 端口的本地服务后执行 `node tools/ecology-firmware-smoke.mjs`
（需 `.toolchains/browser` 下安装 Playwright 和本机 Chrome；可用 `ECO_TEST_URL` 覆盖地址）。
脚本校验真实固件屏幕文字与按键流程，覆盖启动、演示环境切换、命名、NVS 保存、收藏浏览和改名。
