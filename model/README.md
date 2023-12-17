# Model Setup

<img src="../assets/mistral.png" width="300" align="right">

This is the instruction guide to setup the model which powers Ada in this project. It is powered by the [Mistral-7B model](https://mistral.ai/news/announcing-mistral-7b/) that is finetuned for this specialized task using Low-rank adaptation (LoRA. If you follow all the steps you should be able to replicate the results.

<img src="../assets/mlx.png" width="300" align="left">

This model is fine-tuned and inferenced using [MLX](https://github.com/ml-explore/mlx), Apple ML Research's new machine learning library built for native computation on apple products. This part of the project is inspired from an example they provided fine-tuning LLaMa.

## Contents

* [Setup](#Setup)
* [Run](#Run)
  * [Fine-tune](#Fine-tune)
  * [Evaluate](#Evaluate)
  * [Generate](#Generate)

## Setup 

Navigate to the model directory
```
cd model
```

Install the dependencies:

```
pip install -r requirements.txt
```

Next, download and convert the model. The Mistral weights can be downloaded with:

```
curl -O https://files.mistral-7b-v0-1.mistral.ai/mistral-7B-v0.1.tar
tar -xf mistral-7B-v0.1.tar
```

Convert the model with:

```
python convert.py \
    --torch-model mistral-7B-v.01 \
    --mlx-model mlx-model
```

## Run

The main script is `lora.py`. To see a full list of options run

```
python lora.py --help
```

### Fine-tune

To fine-tune a model use:

```
python lora.py --model mlx-model \
               --train \
               --iters 600
               --data=data
```

Note, the model path should have the MLX weights, the tokenizer, and the
`params.json` configuration which will all be output by the `convert.py` script.

By default, the adapter weights are saved in `adapters.npz`. You can specify
the output location with `--adapter-file`.

You can resume fine-tuning with an existing adapter with `--resume-adapter-file
<path_to_adapters.npz>`. 

### Evaluate

To compute test set perplexity use

```
python lora.py --model <path_to_model> \
               --adapter-file <path_to_adapters.npz> \
               --test 
```

### Generate

For generation use

```
python lora.py --model <path_to_model> \
               --adapter-file <path_to_adapters.npz> \
               --num-tokens 50 \
               --prompt "Q: (INSERT PROMPT HERE) A: "
```


[^lora]: Refer to the [arXiv paper](https://arxiv.org/abs/2106.09685) for more details on LoRA.
[^llama]: Refer to the [arXiv paper](https://arxiv.org/abs/2302.13971) and [blog post](https://ai.meta.com/blog/large-language-model-llama-meta-ai/) for more details.
[^mistral]: Refer to the [blog post](https://mistral.ai/news/announcing-mistral-7b/) and [github repository](https://github.com/mistralai/mistral-src) for more details.
[^wikisql]: Refer to the [GitHub repo](https://github.com/salesforce/WikiSQL/tree/master) for more information about WikiSQL.
