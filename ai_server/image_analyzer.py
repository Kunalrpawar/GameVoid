"""
GameVoid Engine — Image Analyzer (Gemini Vision API)
=====================================================
Sends an image to the Google Gemini Vision API to identify the object,
returning structured JSON with object type, category, and description.
"""

import base64
import json
import os
import re
import requests
from pathlib import Path


class ImageAnalyzer:
    """Analyzes images using Google Gemini Vision API."""

    def __init__(self, api_key: str, model: str = "gemini-2.5-flash-lite"):
        self.api_key = api_key
        self.model = model
        self.base_url = "https://generativelanguage.googleapis.com/v1beta/models"

    def analyze(self, image_path: str) -> dict:
        """
        Analyze an image and return structured object information.

        Returns:
            dict: {"object": "chair", "category": "furniture", "description": "..."}
        """
        if not os.path.exists(image_path):
            return {"error": f"Image not found: {image_path}"}

        # Read and encode image
        with open(image_path, "rb") as f:
            image_data = base64.b64encode(f.read()).decode("utf-8")

        # Determine MIME type
        ext = Path(image_path).suffix.lower()
        mime_map = {
            ".png": "image/png",
            ".jpg": "image/jpeg",
            ".jpeg": "image/jpeg",
            ".bmp": "image/bmp",
            ".webp": "image/webp",
            ".gif": "image/gif",
        }
        mime_type = mime_map.get(ext, "image/png")

        # Build Gemini Vision API request
        url = f"{self.base_url}/{self.model}:generateContent?key={self.api_key}"

        payload = {
            "contents": [
                {
                    "parts": [
                        {
                            "text": (
                                "Analyze this image and identify the main object. "
                                "Respond ONLY with a JSON object (no markdown, no code fences) with these fields:\n"
                                '  "object": the specific object name (e.g., "chair", "car", "tree"),\n'
                                '  "category": the object category (e.g., "furniture", "vehicle", "nature"),\n'
                                '  "description": a short 1-sentence description of the object\n'
                                "Example response:\n"
                                '{"object": "wooden_chair", "category": "furniture", '
                                '"description": "A wooden dining chair with a curved backrest"}'
                            )
                        },
                        {
                            "inline_data": {
                                "mime_type": mime_type,
                                "data": image_data,
                            }
                        },
                    ]
                }
            ],
            "generationConfig": {"temperature": 0.3, "maxOutputTokens": 256},
        }

        try:
            response = requests.post(
                url,
                json=payload,
                headers={"Content-Type": "application/json"},
                timeout=30,
            )
            response.raise_for_status()
            data = response.json()

            # Extract text from Gemini response
            text = ""
            if "candidates" in data and len(data["candidates"]) > 0:
                candidate = data["candidates"][0]
                if "content" in candidate and "parts" in candidate["content"]:
                    for part in candidate["content"]["parts"]:
                        if "text" in part:
                            text = part["text"]
                            break

            if not text:
                return {
                    "object": "unknown_object",
                    "category": "unknown",
                    "description": "Could not analyze image",
                }

            # Parse JSON from response (strip code fences if present)
            text = text.strip()
            text = re.sub(r"^```json\s*", "", text)
            text = re.sub(r"^```\s*", "", text)
            text = re.sub(r"\s*```$", "", text)
            text = text.strip()

            result = json.loads(text)

            # Sanitize the object name for use as filename
            obj_name = result.get("object", "unknown_object")
            obj_name = re.sub(r"[^a-zA-Z0-9_]", "_", obj_name).lower()
            result["object"] = obj_name

            return result

        except requests.exceptions.RequestException as e:
            print(f"[ImageAnalyzer] API request failed: {e}")
            return {
                "object": "unknown_object",
                "category": "unknown",
                "description": f"API error: {str(e)}",
            }
        except json.JSONDecodeError:
            # If Gemini didn't return valid JSON, extract what we can
            print(f"[ImageAnalyzer] Failed to parse JSON from: {text}")
            return {
                "object": "detected_object",
                "category": "unknown",
                "description": text[:100] if text else "Parse error",
            }
