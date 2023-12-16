import json
import random

def generate_random_data(num_rows, file_name):
    first_names = ["John", "Jane", "Alice", "Bob", "Charlie", "Diana", "Ethan", "Fiona", "George", "Hannah", "Maria", "Mohammed", "Jose", "Ali", "Yan", "Ana", "Elena"]
    email_domains = ["@gmail.com", "@yahoo.com", "@outlook.com"]

    with open(file_name, 'w') as file:
        for _ in range(num_rows):
            name = random.choice(first_names)
            id = random.randint(100, 999)
            email_domain = random.choice(email_domains)
            email = f"{name.lower()}{id}{email_domain}"

            user_question = f"Insert {name} with id {id} with the email {email}"
            assistant_answer = f"insert {name} {id} {email}"

            formatted_text = f"Q: {user_question} \nA: {assistant_answer}"
            data = {"text": formatted_text}

            json_line = json.dumps(data)
            file.write(json_line + '\n')

# Generate data for each file
generate_random_data(500, 'train1.jsonl')
generate_random_data(50, 'valid1.jsonl')
generate_random_data(50, 'test1.jsonl')
