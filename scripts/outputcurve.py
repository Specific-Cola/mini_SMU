import serial
import matplotlib.pyplot as plt
import csv

# 设置串口参数
ser = serial.Serial('/dev/cu.usbserial-140', 9600, timeout=1)  # 根据实际情况修改串口号和波特率

# 初始化绘图
plt.ion()
fig, ax = plt.subplots()
plt.xlabel('Vsd (V)')
plt.ylabel('Current (A)')
plt.title('Output Curve')

# 初始化CSV文件写入
csv_filename = "output_DN2540_Vsd-0.1to0.1_step0.01.csv"
csv_file = open(csv_filename, 'w', newline='')
csv_writer = csv.writer(csv_file)
csv_writer.writerow(['Vg (V)', 'Vsd (V)', 'Isd (A)'])

# 设置需要接收的栅压组数和每个栅压下的数据数
num_vg_groups = 11  # 根据实际情况设置
num_data_per_vg = 21  # 每个栅压组的数据点数，根据实际情况设置

# 初始化计数器和栅压值集合
group_count = 0  # 栅压组计数器
data_count = 0  # 每个栅压下数据点计数器
current_vg = None  # 当前的栅压值
first_data_received = False  # 是否已经接收到第一组数据

# 设置颜色循环
color_cycle = plt.rcParams['axes.prop_cycle'].by_key()['color']
color_index = 0

try:
    while group_count < num_vg_groups:  # 设置需要接收的栅压组数
        ser_bytes = ser.readline()
        decoded_bytes = ser_bytes.decode('utf-8', 'ignore').strip()
        try:
            vg, vsd, isd = map(float, decoded_bytes.split(', '))

            if not first_data_received:
                current_vg = vg
                first_data_received = True
                color = color_cycle[color_index % len(color_cycle)]
                ax.plot([], [], 'o', label=f'Vg = {current_vg} V', color=color)  # 第一次绘制时添加图例
                color_index += 1

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
                if group_count < num_vg_groups:
                    current_vg = None  # 重置当前栅压值，以便下一组数据更新图例
                    first_data_received = False  # 重置第一次接收数据标志
                    ax.legend(loc='upper left')  # 更新图例
                    plt.draw()  # 重新绘制以更新图例

        except ValueError:
            print("ValueError occurred")
except KeyboardInterrupt:
    print("程序终止")
finally:
    ser.close()
    csv_file.close()
    ax.legend(loc='upper left')  # 在循环结束后添加总的图例
    plt.ioff()
    plt.show()
