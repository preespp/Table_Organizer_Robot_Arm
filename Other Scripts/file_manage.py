import os
import sys

# Check for dataset directory argument
if len(sys.argv) < 2:
    sys.exit(1)

dataset_dir = sys.argv[1]
labels_dir = os.path.join(dataset_dir, "labels/")
images_dir = os.path.join(dataset_dir, "images/")

image_extensions = ['.jpg']

# # Process each label file
# for label_filename in os.listdir(labels_dir):
#     if label_filename.endswith(".txt"):
#         base_name = os.path.splitext(label_filename)[0]

#         # Check for matching image file
#         image_found = any(
#             os.path.exists(os.path.join(images_dir, base_name + ext)) 
#             for ext in image_extensions
#         )

#         # Delete label if image not found
#         if not image_found:
#             label_path = os.path.join(labels_dir, label_filename)
#             print(f"Deleting label file: {label_path}")
#             os.remove(label_path)

# Process each image file
for image_filename in os.listdir(images_dir):
    if any(image_filename.endswith(ext) for ext in image_extensions):
        base_name = os.path.splitext(image_filename)[0]
        label_path = os.path.join(labels_dir, base_name + ".txt")

        # Delete image if label does not exist
        if not os.path.exists(label_path):
            image_path = os.path.join(images_dir, image_filename)
            print(f"Deleting image file: {image_path} (no corresponding label)")
            os.remove(image_path)

print("Check complete.")
