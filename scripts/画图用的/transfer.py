import serial
import matplotlib.pyplot as plt
import csv
import time

# 设置串口参数
ser = serial.Serial('/dev/cu.usbserial-0001', 115200, timeout=1)  # 根据实际情况修改串口号和波特率
ser.reset_input_buffer()  # 清空输入缓冲区

# 设置初始化参数
Vsd = 1
Vg_START = -50
Vg_END = 50
Vg_STEP = 1

# 向串口发送初始化命令
init_command = "Transfer\n"  # 初始化命令
ser.write(init_command.encode())

# 发送初始化参数
init_params = f"{Vsd},{Vg_START},{Vg_END},{Vg_STEP}\n"
ser.write(init_params.encode())

# 初始化数据列表
voltages = []
currents = []

# 初始化绘图
plt.ion()
fig, ax = plt.subplots()
line, = ax.plot(voltages, currents, 'r-')
plt.xlabel('Vg (V)')
plt.ylabel('Current (nA)')
plt.title('Transfer Curve')

# 初始化CSV文件写入
csv_filename = "Transfer-MOS2.csv"
csv_file = open(csv_filename, 'w', newline='')
csv_writer = csv.writer(csv_file)
csv_writer.writerow(['Vg (V)', 'Current (nA)'])

try:
    data_count = 0
    start_reading = False  # 是否开始记录数据的标志

    while True:
        ser_bytes = ser.readline()
        decoded_bytes = ser_bytes.decode('utf-8', errors='ignore').strip()

        try:
            # 解析数据
            vg, current = map(float, decoded_bytes.split(','))

            # 检查是否到达起始电压
            if vg == Vg_START:
                start_reading = True  # 开始记录数据

            if start_reading:
                voltages.append(vg)
                currents.append(current)
                data_count += 1

                # 更新绘图
                line.set_xdata(voltages)
                line.set_ydata(currents)
                ax.relim()
                ax.autoscale_view()
                plt.draw()
                plt.pause(0.1)

                # 打印数据并写入CSV
                print(f"Received: Vg = {vg}, Current = {current}")
                csv_writer.writerow([vg, current])

                # 检查是否完成所有数据点采集
                if data_count == (Vg_END - Vg_START) / Vg_STEP * 2 + 1:
                    print("数据采集完成")
                    break

        except ValueError:
            # 忽略无效数据
            continue

except KeyboardInterrupt:
    print("程序被用户终止")
finally:
    time.sleep(5)
    ser.close()
    csv_file.close()
    plt.ioff()
    plt.show()
