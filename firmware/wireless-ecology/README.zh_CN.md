[English](README.md)

# 无线生态观察器

ESP32-C3 / 240 × 320 / ESP-IDF 5.5.3 独立固件。参考 https://github.com/folotoy/ai-passport
的 Wi-Fi STA 扫描流程，复用显示与 ADC 按键 BSP，基准提交
`df3990726e3751fadaaaa703a480dbba6e13c61b`，许可证见 `components/bsp/LICENSE`。

## 当前交互

开机直接持续观察真实 2.4 GHz Wi-Fi 环境，每 5 秒自动发起一次被动扫描；扫描不重叠，
如果一次扫描超过 5 秒，则完成后再开始下一次。扫描期间显示上次结果，失败保留景观并自动重试。
界面显示 AUTO 5S、更新序号、扫描状态或下次扫描倒计时，不需要按键才能刷新。
已移除盖章、命名、收藏界面及其 NVS 读写。旧版收藏数据不会主动擦除。

- 树木使用像素松树轮廓：2–4 层展开枝叶、树冠明暗、棕色树干与外伸树根；树尖仍对应信号高度。
- 横轴 CH 1–14 从左到右固定排列。上下键选择信道，高亮对应列。
- 每个信道一棵树，表示该信道中保留热点的最强平滑 RSSI；所有树使用相同形状、宽度、基线和高度刻度。
- 左侧提供 -30 / -60 / -90 dBm 参考线：数值越接近 0，树越高。高度在 -95 到 -30 dBm 之间映射为 8–60 逻辑像素，越界截断。
- 信道 1–4 为绿色、5–9 为琥珀色、10–14 为蓝色；地面色带与树木对应。
- AP 行显示每个信道保留的热点数量；底部显示所选信道的热点数及 PEAK dBm。
- 追踪最多 24 个热点，顶部 AP 为扫描总数；超过上限时，分信道统计仅代表追踪子集。
- 按 BSSID 散列识别热点，RSSI 使用 3:1 平滑。首次漏扫保留，连续两次成功扫描未发现后移除。
  一个信道所有保留热点都漏扫一次时，树变灰且不显示生物；统计与 PEAK 可能包含一轮旧数据。
- 信号与信道用于艺术映射，不能用于判断人数或准确衡量信道拥堵。

| 按键 | 行为 |
| --- | --- |
| UP / ↑ | 选择前一个信道，循环 |
| DOWN / ↓ | 选择后一个信道，循环 |
| 长按 DOWN 约 2 秒 | 切换真实扫描 / DEMO SYNTHETIC |
| OK / Enter | 真实模式遵守 5 秒间隔请求扫描；演示模式立即切换下一组数据 |

演示模式每 5 秒自动循环 3 组人工热点环境，真实和演示数据独立；切入演示前已经开始的真实扫描会完成。
界面使用英文短标签，不需要互联网或 AI 服务。

## 模拟器测试与固件

根目录执行 `npm start`，打开 http://127.0.0.1:4190，加载
`public/assets/firmware/wireless-ecology.bin`。这是地址 0x0 的 Full Flash 合并镜像。
模拟器只提供虚拟 Wi-Fi，不能扫描电脑周围热点。建议先不按任何键，确认更新序号持续增加；
再长按 DOWN 进入演示，等待景观自动变化，上下切换信道查看 PEAK 数值。

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
启动 4193 端口的服务后执行 `node tools/ecology-firmware-smoke.mjs` 可验证无需按键的连续扫描、
自动演示及信道选择，需 `.toolchains/browser` 下的 Playwright 和本机 Chrome；地址可由 `ECO_TEST_URL` 指定。
真机扫描覆盖率、移动体验、屏幕可读性及 5 秒扫描间隔的耗电仍需实物验证。
