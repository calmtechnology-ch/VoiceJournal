cd VoiceJournal_CoreS3
pio run -e m5stack-cores3 -t upload

pio run -e m5stack-cores3 -t upload -t monitor

pio device monitor -b 115200

(Si besoin de forcer le port de téléversement : ajoutez --upload-port COM9)