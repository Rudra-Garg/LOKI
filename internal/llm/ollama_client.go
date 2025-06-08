package llm

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"
	"time"
)

type OllamaClient struct {
	BaseURL string
	Model   string
	Timeout time.Duration
}

func New(baseURL, model string) *OllamaClient {
	return &OllamaClient{baseURL, model, 5 * time.Second}
}

func (c *OllamaClient) Query(prompt string) (map[string]interface{}, error) {
	payload := map[string]string{"model": c.Model, "prompt": prompt}
	body, _ := json.Marshal(payload)
	req, _ := http.NewRequest("POST", c.BaseURL+"/api/completions", bytes.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	client := http.Client{Timeout: c.Timeout}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer func(Body io.ReadCloser) {
		err := Body.Close()
		if err != nil {

		}
	}(resp.Body)
	var out map[string]interface{}
	err = json.NewDecoder(resp.Body).Decode(&out)
	if err != nil {
		return nil, err
	}
	return out, nil
}
