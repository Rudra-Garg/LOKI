#ifndef LOKI_EMBEDDINGMODEL_H
#define LOKI_EMBEDDINGMODEL_H

#include <string>
#include <vector>
#include <memory>

class EmbeddingModel {
public:
    EmbeddingModel();

    ~EmbeddingModel();

    // The vocab is part of the GGUF file, so we only need the model path.
    bool load(const std::string &model_path) const;

    std::vector<float> get_embeddings(const std::string &text);

    // A modern C++ way to create an instance
    static std::unique_ptr<EmbeddingModel> create(const std::string &model_path);

private:
    // PIMPL (Pointer to Implementation) to hide implementation details
    struct EmbeddingModelImpl;
    std::unique_ptr<EmbeddingModelImpl> pimpl;
};

#endif //LOKI_EMBEDDINGMODEL_H
