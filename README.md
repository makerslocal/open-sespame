This is an ESP8266-based controller for things that can be turned on and off. It's compatible with the [RQ](https://256.makerslocal.org/wiki/RQ) platform for automation.

### Build and Deploy
    pip install platformio
    cp src/config.example.h src/config.h
    platformio run
    platformio run -t upload
