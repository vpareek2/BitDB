from openai import OpenAI
import os
import sys

client = OpenAI()

def get_response(prompt):
    response = client.chat.completions.create(
        model="ft:gpt-3.5-turbo-1106:personal::8TmYJnni",
        messages=[
            {"role": "system", "content": "You are a database query chatbot meant to go from natural language to queries."},
            {"role": "user", "content": prompt}
        ],
        temperature=1,
        max_tokens=256,
        top_p=1,
        frequency_penalty=0,
        presence_penalty=0
    )
    # Extract the response content
    response_content = response.choices[0].message.content.strip()

    # Check and return only the expected command
    # This can be adjusted based on your expected output and command types
    if "select" in response_content.lower():
        return "select"
    else:
        return "unrecognized"

if __name__ == "__main__":
    user_input = sys.argv[1]
    result = get_response(user_input)
    print(result)  # The result will be read by the C program
