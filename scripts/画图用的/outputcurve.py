import serial
import matplotlib.pyplot as plt
import csv
import time

# 设置串口参数
ser = serial.Serial('', 115200, timeout=1)  # 根据实际情况修改串口号和波特率
ser.reset_input_buffer()
ser.reset_output_buffer()

# 设置初始化参数
Vsd_START = -1
Vsd_END = 1
Vsd_STEP = 0.05
Vg_START = -5
Vg_END = 5
Vg_STEP = 5

# 发送初始化命令和参数，确保有足够延时
ser.write("Output\n".encode())
time.sleep(0.1)  # 给单片机足够时间处理命令
ser.write(f"{Vsd_START},{Vsd_END},{Vsd_STEP},{Vg_START},{Vg_END},{Vg_STEP}\n".encode())
time.sleep(0.1)  # 确保单片机能正确解析参数

# 初始化绘图
plt.ion()
fig, ax = plt.subplots()
plt.xlabel('Vsd (V)')
plt.ylabel('Current (nA)')
plt.title('Output Curve')

# 初始化CSV文件写入
csv_filename = "Output_MoS2.csv"
csv_file = open(csv_filename, 'w', newline='')
csv_writer = csv.writer(csv_file)
csv_writer.writerow(['Vg (V)', 'Vsd (V)', 'Isd (A)'])

# 设置需要接收的栅压组数和每个栅压下的数据数
num_vg_groups = int((Vg_END - Vg_START) / Vg_STEP + 1)  # 根据实际情况设置
num_data_per_vg = int((Vsd_END - Vsd_START) / Vsd_STEP * 2 + 2)  # 每个栅压组的数据点数
print(num_data_per_vg)

# 初始化计数器和栅压值集合
group_count = 0  # 栅压组计数器
data_count = 0  # 每个栅压下数据点计数器
current_vg = None  # 当前的栅压值

# 设置颜色循环
color_cycle = plt.rcParams['axes.prop_cycle'].by_key()['color']
color_index = 0

# 浮点数比较的误差范围
Vg_tolerance = 0.5
Vsd_tolerance = 0.01

# 初始化一个字典，用于存储每个 Vg 的颜色
vg_color_map = {}

try:
    start_reading = False  # 是否开始记录数据的标志
    while group_count < num_vg_groups:  # 设置需要接收的栅压组数
        ser_bytes = ser.readline()
        decoded_bytes = ser_bytes.decode('utf-8', 'ignore').strip()
        try:
            vg, vsd, isd = map(float, decoded_bytes.split(', '))

            # 从 Vg = Vg_START 且 Vsd = -Vsd_STEP 开始记录
            if not start_reading and abs(vg - Vg_START) < Vg_tolerance and abs(vsd - -Vsd_STEP) < Vsd_tolerance:
                start_reading = True

            if start_reading:
                # 如果当前 Vg 没有分配颜色，则分配一个新的颜色
                if vg not in vg_color_map:
                    vg_color_map[vg] = color_cycle[color_index % len(color_cycle)]
                    color_index += 1

                color = vg_color_map[vg]

                # 当 Vg 发生变化时，更新图例
                if current_vg != vg:
                    if current_vg is not None and vg < current_vg:  # 如果 Vg 变小，停止接收
                        print("Vg 减小，停止接收数据。")
                        break
                    current_vg = vg
                    ax.plot([], [], 'o', label=f'Vg = {current_vg} V', color=color)

                ax.plot(vsd, isd, 'o', color=color, alpha=0.5)  # 绘制数据点，指定颜色和透明度
                ax.relim()
                ax.autoscale_view()
                plt.draw()
                plt.pause(0.5)  # 控制图像刷新的间隔
                print(decoded_bytes)
                csv_writer.writerow([vg, vsd, isd])

                data_count += 1
                if data_count == num_data_per_vg:
                    data_count = 0  # 重置数据点计数器
                    group_count += 1  # 增加栅压组计数器

        except ValueError:
            print("ValueError occurred")
except KeyboardInterrupt:
    print("程序终止")
finally:
    time.sleep(5)
    ser.close()
    csv_file.close()
    ax.legend(loc='upper left')  # 在循环结束后添加总的图例
    plt.ioff()
    plt.show()