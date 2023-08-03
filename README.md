# m1s_ups
저가의 ch55x chip을 사용한 m1s용 smart-ups project.  

ch55x 참조 사이트
* <https://github.com/DeqingSun/ch55xduino>
* <https://cdn.jsdelivr.net/gh/DeqingSun/ch55xduino@playground>
* <https://github.com/topics/ch552>

## Install Package
* arduino-cli install
```
  root@odroid:~# mkdir cli
  root@odroid:~/cli# cd cli
  root@odroid:~/cli# curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
  root@odroid:~/cli# cp ./bin/arduino-cli /bin/
```

* config파일 생성 및 보드 설치
```
  root@odroid:~/cli# arduino-cli config init
  root@odroid:~/cli# vi ~/.arduino15/arduino-cli.yaml
```

* board package urls 추가
```
  additional_urls:
    - https://raw.githubusercontent.com/DeqingSun/ch55xduino/ch55xduino/package_ch55xduino_mcs51_index.json
```

* board package index file download
```
  root@odroid:~/cli# cd ~/.arduino15
  root@odroid:~/.arduino15# wget https://raw.githubusercontent.com/DeqingSun/ch55xduino/ch55xduino/package_ch55xduino_mcs51_index.json
```

* board package & core install
```
  root@odroid:~/.arduino15# arduino-cli core install CH55xDuino:mcs51
```
 
## Compile & Download
```
  root@odroid:~# git clone https://github.com/charles-park/m1s_ups
  root@odroid:~# cd m1s_ups
  # compile only
  root@odroid:~/m1s_ups# arduino-cli compile -b CH55xDuino:mcs51:ch552 ups_fw
  # compile & download
  root@odroid:~/m1s_ups# arduino-cli compile -b CH55xDuino:mcs51:ch552 ups_fw -u -p /dev/ttyACM0

```
## ch55x 전용 tool을 사용한 download
* bootloader rule 복사
```
  root@odroid:~/cli# wget https://cdn.jsdelivr.net/gh/DeqingSun/ch55xduino@playground/ch55xduino/tools/linux_arm/99-ch55xbl.rules
  root@odroid:~/cli# cp 99-ch55xbl.rules /etc/udev/rules.d/
```
* 전용 tool download
```
  root@odroid:~/cli# wget https://cdn.jsdelivr.net/gh/DeqingSun/ch55xduino@playground/ch55xduino/tools/linux_arm/vnproch55x
  root@odroid:~/cli# chmod 777 ./vnproch55x
  root@odroid:~/cli# cp vnproch55x /bin/
```
## Hex file 생성위치
```
  /tmp/arduino/sketches/***/ups_fw.hex
```
