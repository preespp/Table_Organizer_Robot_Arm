#!/usr/bin/env python3

import json
import time

def main():
    while True:
        try:
            with open("object_positions.json", "r") as f:
                data = json.load(f)
                print("Original Positions:", data["original"])
                print("Current Positions:", data["current"])
        except FileNotFoundError:
            print("Waiting for data...")
    
        time.sleep(1)

if __name__ == "__main__":
    main()