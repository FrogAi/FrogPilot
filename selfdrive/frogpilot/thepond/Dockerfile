FROM python:3.11

# Install Python
RUN apt-get update -y && \
    apt-get install -y python3-pip python3-dev ffmpeg

# Install Python dependencies with pip
RUN pip3 install --upgrade pip && \
    pip3 install flask requests gevent

# Copy the current directory contents into the container at /app
COPY . /thepond

CMD ["python3", "-m", "thepond.thepond"]
