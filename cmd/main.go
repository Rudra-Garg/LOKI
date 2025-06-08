package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/Rudra-Garg/LOKI/internal/audio"
	"github.com/Rudra-Garg/LOKI/internal/llm"
)

func main() {
	fmt.Println("🔊 LOKI is listening... Say the wake word!")

	// Channel to capture interrupt
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)

	// Start wake word detection
	go func() {
		for {
			err := audio.ListenForWakeWord(func() {
				fmt.Println("🟢 Wake word detected!")

				transcription, err := audio.TranscribeSpeech()
				if err != nil {
					log.Println("❌ Error in transcription:", err)
					return
				}

				fmt.Println("📜 You said:", transcription)

				response, err := llm.GetLLMResponse(transcription)
				if err != nil {
					log.Println("❌ LLM error:", err)
					return
				}

				fmt.Println("🤖 LOKI:", response)
			})

			if err != nil {
				log.Println("❌ Wake word listener error:", err)
			}

			// Wait a bit before reinitializing
			time.Sleep(1 * time.Second)
		}
	}()

	<-quit
	fmt.Println("\n👋 Exiting LOKI. Goodbye!")
}
