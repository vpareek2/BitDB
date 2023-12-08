import json
import random

def generate_random_data(num_rows, file_name):
    first_names = ["John", "Jane", "Alice", "Bob", "Charlie", "Diana", "Ethan", "Fiona", "George", "Hannah", "Maria", "Mohammed", "Jose", "Ali", "Yan", "Ana", "Elena"]
    email_domains = ["@gmail.com", "@yahoo.com", "@outlook.com"]

    with open(file_name, 'w') as file:
        for _ in range(num_rows):
            name = random.choice(first_names)
            id = f"{random.randint(100, 999):02d}"
            email_domain = random.choice(email_domains)
            email = f"{name.lower()}{id}{email_domain}"

            question = f"Insert {name} with id {id} with the email {email}"
            answer = f"insert {name} {id} {email}"

            data = {"question": question, "answer": answer}
            json_line = json.dumps(data)
            file.write(json_line + '\n')

generate_random_data(400, 'data.jsonl')
