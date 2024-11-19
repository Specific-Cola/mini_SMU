import serial
import matplotlib.pyplot as plt
import csv
import time

# 设置串口参数
ser = serial.Serial('/dev/cu.usbserial-0001', 115200, timeout=1)  # 根据实际情况修改串口号和波特率

# 初始化绘图
plt.ion()
fig, ax = plt.subplots()
plt.xlabel('Vg (V)')
plt.ylabel('Current (A)')
plt.title('Transfer Curve')

# 初始化CSV文件写入
csv_filename = "IV.csv"
csv_file = open(csv_filename, 'w', newline='')
csv_writer = csv.writer(csv_file)
csv_writer.writerow(['Vg (V)', 'Isd (A)'])

# 记录Vg出现的次数
vg_counts = {}

try:
    while True:
        ser_bytes = ser.readline()
        if ser_bytes:
            decoded_bytes = ser_bytes.decode('utf-8', 'ignore').strip()
            try:
                vg, isd = map(float, decoded_bytes.split(', '))

                # 记录vg出现的次数
                if vg in vg_counts:
                    vg_counts[vg] += 1
                else:
                    vg_counts[vg] = 1

                if vg_counts[vg] == 3:
                    print(f"Detected third occurrence of Vg = {vg}. Terminating program.")
                    break
                ax.plot(vg, isd, 'o', color='blue')  # 绘制数据点，指定颜色和透明度
                ax.relim()
                ax.autoscale_view()
                plt.draw()
                plt.pause(0.01)  # 控制图像刷新的间隔
                print(f"Vg: {vg}, Isd: {isd}")
                csv_writer.writerow([vg, isd])


            except ValueError:
                print(f"ValueError occurred: {decoded_bytes}")
        time.sleep(0.1)  # 控制读取数据的间隔
except KeyboardInterrupt:
    print("程序终止")
    ser.close()
    csv_file.close()
    plt.ioff()
    plt.close()
finally:
    ser.close()
    csv_file.close()
    plt.ioff()
    plt.show()
