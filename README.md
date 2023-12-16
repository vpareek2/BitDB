# BitDB           

<img src="other/BrailleSculptLogo.png" width="300" align="right">

A lightweight database written in C with a personal assistant named Ada to help with your query needs.

## Description

BitDB is a very lightweight skeleton database. Stemming from my interest in learning more about both databases and memory operations in C, I created this project by watching Computer Architecture lectures and following cstack's database tutorial. After, I used ruby for testing different database scenarios and edge cases. Then, being a native mac user myself, I used Apple ML Research's brand new MLX library to implement a Mistral-7B model finetuned with lora to be a natural language to code generator. I took heavy inspiration from the llama example from the mlx repository, and adjusted the code to use mistral. (As of Dec 16 a mistral lora example has been added to the repo.) 

## Demo
![](other/Demo.gif)



### Dependencies

* `pip install git+https://github.com/deepmind/dm-haiku`
* `pip install plotnine`
* `pip install pybraille`
* `pip install gradio`


### Executing program

* First navigate to the model folder to set up some pre-requisites for the db to work.
  
```
python run.py
```


## Authors

Contributors name and contact info

ex. Veer Pareek - vpareek2@illinois.edu

## Version History

* 0.1
    * Initial Release

- [ ] Find a more efficient way to save and load model
- [ ] Add a block under the braille so it isn't all seperated
- [ ] Collect more english to braille text
- [ ] Implement Grade 2 Braille translation
- [ ] Write a script to auto 3D Print


## Note about architecture
You might be confused as to why I would use a model as complex as a LSTM. I honestly did this project to learn about the architecture of a LSTM and how to use JAX and Haiku to build it. While they are new languages for me, the documentation was thorough and has the ability to speed up large projects. Also down the line I plan to collect a dataset filled with english to braile sentences and paragraphs to train a Grade 2 Braille model, that can make actual sentences, rather than translations, using contractions, numbers, punctuation, etc. 


