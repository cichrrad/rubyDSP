### Tier 1: Core Audio Preparation (The "Must-Haves")

Before anyone can do math on audio, the audio has to be in the exact format their algorithm expects.

> DONE (Not for 5.1 stuff, but N-channel support)
* **Mono Mixdown (Stereo to Mono):** Most audio analysis and ML models completely ignore stereo sound because processing two channels doubles the compute time.
* *What it is:* A C++ method that averages the left and right channels into a single channel.
* *Ruby API:* `track.to_mono!`


> DONE
* **Peak Amplitude:** * *What it is:* Scanning the entire float array to find the absolute highest value.
* *Why it's useful:* Developers use this to "Normalize" audio (scaling the volume up so the loudest peak hits exactly `1.0` or `-1.0` without clipping).
* *Ruby API:* `track.peak_amplitude`

> DONE (only linear though, low quality but fast)
* **Resampling (Crucial for ML):**
* *What it is:* Changing a 44,100Hz CD-quality file into a 16,000Hz file.
* *Why it's useful:* Almost all speech recognition AI (like Whisper) requires 16kHz audio. Doing this in Ruby is mathematically brutal; `miniaudio` can actually do this for us in C++ during the loading phase.



### Tier 2: Time-Domain Analysis (The Easy Math)

These look at the audio waveform exactly as we loaded it (amplitude over time).
>DONE
* **RMS Energy**

* **Zero-Crossing Rate (ZCR):**
* *What it is:* Counting how many times the audio signal crosses the `0.0` line (going from positive to negative).
* *Why it's useful:* High ZCR means the sound is "noisy" or "scratchy" (like a snare drum, or the 'S' sound in speech). Low ZCR means the sound is tonal/harmonic (like a bass guitar or a vowel). It is the easiest way to build a basic Voice Activity Detector (VAD).
* *Ruby API:* `track.zcr`


* **Silence Trimming:**
* *What it is:* Finding the first and last indices in the vector where the RMS energy exceeds a certain threshold, and cropping the array.
* *Ruby API:* `track.trim_silence(threshold_db: -40)`



### Tier 3: Frequency-Domain Analysis (The "Magic" Tier)

This is where the gem goes from "neat utility" to "powerful DSP library." To do this, we have to integrate the `KissFFT` library to convert our time-domain floats into frequency-domain data (showing us the actual pitches and notes).

* **Short-Time Fourier Transform (STFT):**
* *What it is:* Instead of analyzing the whole song at once, STFT chops the audio into tiny windows (e.g., 20 milliseconds long) and finds the frequencies in each window. This creates a Spectrogram (a 2D map of time vs. frequency).


* **Spectral Centroid:**
* *What it is:* Calculates the "center of mass" of the frequencies.
* *Why it's useful:* It tells you how "bright" a sound is. A trumpet has a high centroid; a kick drum has a low one.



### Tier 4: The Machine Learning Holy Grail


* **Mel-Frequency Cepstral Coefficients (MFCCs):**
* *What it is:* A highly compressed mathematical representation of the "shape" of a sound, designed to mimic how the human ear actually hears.
* *Why it's useful:* If you want to train a neural network to recognize a dog bark vs. a car horn, or detect a specific speaker's voice, you don't feed it raw audio floats—you feed it MFCCs.