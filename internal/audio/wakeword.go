package audio

import (
	"fmt"
	"github.com/Picovoice/porcupine/binding/go"
	"github.com/joho/godotenv"
	"os"
)

func DetectWakeWord() error {

	err := godotenv.Load()

	if err != nil {
		return fmt.Errorf("error loading .env file: %w", err)
	}

	PorcupineAccessKey := os.Getenv("PORCUPINE_ACESS_KEY")

	p, err := porcupine.NewPorcupine("models/porcupine/porcupine_params.pv", "models/porcupine/Hey-Loki.ppn", PorcupineAccessKey)

	if err != nil {
		return err
	}

	for {
		sample := make([]int16, porcupine.FrameLength)
		detected, err := p.Process(sample)
		if err != nil {
			return err
		}
		if detected {
			fmt.Println("Wake word detected!")

		}
	}
}
