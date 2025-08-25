import threading
import time
import requests
import random
import statistics

# Configuration
num_clients = 50  # Number of simulated clients
url = "http://example.com"  # Replace with the actual server URL
response_times = []

def simulate_client():
    start_time = time.time()
    try:
        response = requests.get(url)
        response.raise_for_status()  # Raise an error for bad responses
        elapsed_time = time.time() - start_time
        response_times.append(elapsed_time)
    except requests.exceptions.RequestException as e:
        print(f"Request failed: {e}")

def main():
    threads = []
    for _ in range(num_clients):
        thread = threading.Thread(target=simulate_client)
        threads.append(thread)
        thread.start()
        time.sleep(random.uniform(0.1, 0.5))  # Staggered start

    for thread in threads:
        thread.join()

    # Performance report
    if response_times:
        avg_response_time = statistics.mean(response_times)
        max_response_time = max(response_times)
        min_response_time = min(response_times)
        print(f"Average response time: {avg_response_time:.2f}s")
        print(f"Max response time: {max_response_time:.2f}s")
        print(f"Min response time: {min_response_time:.2f}s")
    else:
        print("No successful requests were made.")

if __name__ == "__main__":
    main()
