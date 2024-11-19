import csv
import matplotlib.pyplot as plt

# 读取CSV文件数据
csv_filename = "0628MOS2_output_±Vsd1_Irange1e-06.csv"

data = {}

with open(csv_filename, 'r', encoding='gbk') as csv_file:  # 使用gbk编码打开文件
    csv_reader = csv.reader(csv_file)
    next(csv_reader)  # 跳过标题行
    for row in csv_reader:
        vg, vsd, isd = map(float, row)
        if vg not in data:
            data[vg] = {'vsd': [], 'isd': []}
        data[vg]['vsd'].append(vsd)
        data[vg]['isd'].append(isd)

# 初始化绘图
fig, ax = plt.subplots(figsize=(10, 7.5))  # 设置长宽比为 4:3

# 设置字体属性
title_font = {'family': 'Arial', 'color': 'black', 'weight': 'bold', 'size': 20}
label_font = {'family': 'Arial', 'color': 'black', 'weight': 'normal', 'size': 14}
tick_font = {'family': 'Arial', 'size': 12}  # 刻度标签字体属性
legend_font = {'family': 'Arial', 'size': 12}  # 图例字体属性

# 设置标题和轴标签
plt.xlabel('Vsd (V)', fontdict=label_font)
plt.ylabel('Current (A)', fontdict=label_font)
plt.title('Output Characteristic Curve', fontdict=title_font)

# 设置刻度标签字体
ax.tick_params(axis='x', labelsize=12)
ax.tick_params(axis='y', labelsize=12)
plt.xticks(fontname=tick_font['family'])
plt.yticks(fontname=tick_font['family'])

# 绘制数据
for vg, values in data.items():
    ax.plot(values['vsd'], values['isd'], label=f'Vg = {vg} V')

# 添加图例
ax.legend(loc='upper left', prop=legend_font)

# 显示图像
plt.tight_layout()  # 自动调整子图参数，以便图像填充整个图像区域
plt.show()
