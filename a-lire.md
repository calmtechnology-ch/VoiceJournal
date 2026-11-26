cd VoiceJournal_CoreS3
pio run -e m5stack-cores3 -t upload

pio run -e m5stack-cores3 -t upload -t monitor

pio device monitor -b 115200

(Si besoin de forcer le port de téléversement : ajoutez --upload-port COM9)

cd c:\Users\phall\Documents\Antigravity-5.\VoiceJournal\VoiceJournal_CoreS3
pio run -e m5stack-cores3 -t upload
pio device monitor -b 115200