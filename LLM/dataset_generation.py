from openai import OpenAI
client = OpenAI()

response = client.chat.completions.create(
  model="gpt-3.5-turbo",
  messages=[
    {
      "role": "system",
      "content": "You generate data from natural language to a database language, the syntax is denoted by triple backticks.\n\nReply with only the generated rows in the following syntax.\n\n\n```\n{\"Insert Katy with id 01 with the email katy01@email.com\" :insert katy 01 katy@email.com}\n```"
    },
    {
      "role": "user",
      "content": "Generate 100 random row"
    },
    {
      "role": "assistant",
      "content": "```\n{\"Insert John with id 02 with the email john02@email.com\" :insert John 02 john02@email.com}\n{\"Insert Lisa with id 03 with the email lisa03@email.com\" :insert Lisa 03 lisa03@email.com}\n{\"Insert Mark with id 04 with the email mark04@email.com\" :insert Mark 04 mark04@email.com}\n{\"Insert Jane with id 05 with the email jane05@email.com\" :insert Jane 05 jane05@email.com}\n{\"Insert Mike with id 06 with the email mike06@email.com\" :insert Mike 06 mike06@email.com}\n```"
    }
  ],
  temperature=0.51,
  max_tokens=256,
  top_p=1,
  frequency_penalty=0,
  presence_penalty=0
)