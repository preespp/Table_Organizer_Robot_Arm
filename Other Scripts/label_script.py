import os
import sys

# Check for dataset directory argument
if len(sys.argv) < 4:
    sys.exit(1)

dataset_dir = sys.argv[1]
labels_dir = os.path.join(dataset_dir, "labels/")

old_class_id = sys.argv[2]  # Class ID to replace
new_class_id = sys.argv[3]  # New class ID

# Process each label file
for filename in os.listdir(labels_dir):
    if filename.endswith(".txt"):
        file_path = os.path.join(labels_dir, filename)
        # Read label file
        with open(file_path, "r") as file:
            lines = file.readlines()
        
        # Modify class ID
        new_lines = []
        for line in lines:
            parts = line.strip().split()
            if parts:
                if parts[0] == old_class_id:
                    parts[0] = str(new_class_id)
                new_lines.append(" ".join(parts))
        
        # Write back modified label
        with open(file_path, "w") as file:
            file.write("\n".join(new_lines) + "\n")

print("Class ID update complete.")
