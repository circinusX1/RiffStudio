# RiffTabs (for Linux Only)

[Buy me a coffee](https://www.paypal.com/ncp/payment/UY6RJK9AQ6HVC)

### web version:       https://ghitara.com/riff/
   * In the above page there is a link to download latest AppRun image
#### the other branch uses extensia library as the web version.


### This is the desktop version of above riffStudio.
####  using vampp nss* plugins

#### prerequisutes
```
sudo apt update
sudo apt install -y build-essential cmake pkg-config git \
                    libfftw3-dev libyaml-cpp-dev libtag1-dev \
                    libsamplerate0-dev libavcodec-dev libavformat-dev \
                    python3 python3-pip  libeigen3-dev libsoundtouch-dev \
sudo apt install -y libeigen3-dev
```

-- --


<img width="1280" height="829" alt="image" src="https://github.com/user-attachments/assets/ecb74735-5a5f-4ada-9de5-f38cd550849c" />



-- --

<img width="1286" height="839" alt="image" src="https://github.com/user-attachments/assets/cb03f89a-4a77-4892-8e9e-8590ee0369ca" />

-- --- 

Web version:


<img width="1286" height="825" alt="image" src="https://github.com/user-attachments/assets/1adb54fa-8fde-4470-909e-2c6005f19f68" />












#### For the fatser_one branch to install C and headers we need the python crap (use main branch no need for these)
```
git clone https://github.com/MTG/essentia
cd essesntia
python3 waf configure --mode=release --ignore-algos=AudioLoader,MonoLoader
python3 waf
sudo python3 waf install
```







