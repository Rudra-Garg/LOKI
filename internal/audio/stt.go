package audio

import (
	"fmt"
	"github.com/alphacep/vosk-api/go"
)

func Transcribe(buffer []byte) (string, error) {
	model, err := vosk.NewModel("models/vosk/model")
	if err != nil {
		return "", err
	}
	rec, err := vosk.NewRecognizer(model, 16000.0)
	if err != nil {
		return "", err
	}
	rec.AcceptWaveform(buffer)
	result := rec.Result()
	fmt.Println("Transcription:", result)
	return result, nil
}
