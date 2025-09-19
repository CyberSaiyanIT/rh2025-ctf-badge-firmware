FROM node:latest

RUN npm install uglify-js sass @node-minify/cli @node-minify/html-minifier -g
ADD docker-script.sh /docker-script.sh

RUN chmod +x /docker-script.sh

ENTRYPOINT ["sh" , "/docker-script.sh" ]
