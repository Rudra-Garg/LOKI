#include "loki/core/EmbeddingModel.h"
#include <iostream>
#include <vector>
#include <stdexcept>


#include "llama_cpp/include/llama.h"


struct EmbeddingModel::EmbeddingModelImpl {
    llama_model *model = nullptr;
    llama_context *ctx = nullptr;


    ~EmbeddingModelImpl() {
        if (ctx) {
            llama_free(ctx);
            ctx = nullptr;
        }
        if (model) {
            llama_model_free(model);
            model = nullptr;
        }


        llama_backend_free();
    }
};


EmbeddingModel::EmbeddingModel() : pimpl(new EmbeddingModelImpl()) {
}

EmbeddingModel::~EmbeddingModel() = default;

bool EmbeddingModel::load(const std::string &model_path) const {
    std::cout << "LOKI: Initializing LLaMA backend for embeddings..." << std::endl;

    llama_backend_init();

    auto mparams = llama_model_default_params();

    pimpl->model = llama_model_load_from_file(model_path.c_str(), mparams);

    if (!pimpl->model) {
        std::cerr << "Error: could not load embedding model from " << model_path << std::endl;
        return false;
    }

    auto cparams = llama_context_default_params();
    cparams.n_ctx = 512;
    cparams.n_batch = 512;

    cparams.embeddings = true;


    pimpl->ctx = llama_init_from_model(pimpl->model, cparams);

    if (!pimpl->ctx) {
        std::cerr << "Error: could not create llama context for embedding model" << std::endl;
        llama_model_free(pimpl->model);
        pimpl->model = nullptr;
        return false;
    }

    std::cout << "LOKI: Embedding model loaded successfully." << std::endl;
    return true;
}

std::vector<float> EmbeddingModel::get_embeddings(const std::string &text) {
    if (!pimpl->ctx || !pimpl->model) {
        return {};
    }


    auto tokens_list = std::vector<llama_token>(text.size() + 2);
    const llama_vocab *vocab = llama_model_get_vocab(pimpl->model);

    int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens_list.data(), tokens_list.size(),
                                  true, false);
    if (n_tokens < 0) {
        std::cerr << "Error: LLaMA tokenization failed." << std::endl;
        return {};
    }
    tokens_list.resize(n_tokens);


    llama_memory_clear(llama_get_memory(pimpl->ctx), true);


    if (llama_encode(pimpl->ctx, llama_batch_get_one(tokens_list.data(), n_tokens))) {
        std::cerr << "Error: LLaMA llama_encode failed." << std::endl;
        return {};
    }


    const int n_embed = llama_model_n_embd(pimpl->model);
    const float *embeddings_ptr = llama_get_embeddings_seq(pimpl->ctx, 0);
    if (!embeddings_ptr) {
        std::cerr << "Error: llama_get_embeddings returned null pointer." << std::endl;
        return {};
    }


    return std::vector<float>(embeddings_ptr, embeddings_ptr + n_embed);
}


std::unique_ptr<EmbeddingModel> EmbeddingModel::create(const std::string &model_path) {
    auto model = std::make_unique<EmbeddingModel>();
    if (model->load(model_path)) {
        return model;
    }
    return nullptr;
}
