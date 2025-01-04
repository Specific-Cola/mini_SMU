import serial
import matplotlib.pyplot as plt
import csv

# 设置初始化参数
Vsd_START = -1
Vsd_END = 1
VsdSTEP = 0.1

# 设置串口参数
ser = serial.Serial('COM5', 115200, timeout=1)  # 根据实际情况修改串口号和波特率
ser.reset_input_buffer()  # Clear the input buffer
# 向串口发送初始化命令
init_command = "IV\n"  # 初始化命令
ser.write(init_command.encode())
# 发送初始化参数
init_params = f"{Vsd_START},{Vsd_END},{VsdSTEP}\n"
ser.write(init_params.encode())


# 初始化数据列表
voltages = []
currents = []

# 初始化绘图
plt.ion()
fig, ax = plt.subplots()
line, = ax.plot(voltages, currents, 'r-')
plt.xlabel('Vsd (V)')
plt.ylabel('Current (nA)')
plt.title('I-V Curve')

# 初始化CSV文件写入
csv_filename = "I-V1210.csv"
csv_file = open(csv_filename, 'w', newline='')
csv_writer = csv.writer(csv_file)
csv_writer.writerow(['Vsd (V)', 'Current (nA)'])

try:
    data_count = 0
    while True:
        ser_bytes = ser.readline()
        decoded_bytes = ser_bytes.decode('utf-8', errors='ignore').strip()

        try:
            # 解析数据
            vsd, current = map(float, decoded_bytes.split(','))
            voltages.append(vsd)
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
            print(f"Received: Vsd = {vsd}, Current = {current}")
            csv_writer.writerow([vsd, current])

            # 检查是否完成所有数据点采集
            if data_count == (Vsd_END-Vsd_START)/VsdSTEP * 2 + 2:
                print("数据采集完成")
                break

        except ValueError:
            # 忽略无效数据
            continue

except KeyboardInterrupt:
    print("程序被用户终止")
finally:
    # 关闭串口和CSV文件
    ser.close()
    csv_file.close()
    plt.ioff()
    plt.show()
