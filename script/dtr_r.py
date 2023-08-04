import serial
ser = serial.Serial('/dev/ttyACM0','1200')
ser.setDTR(0)
ser.close()

