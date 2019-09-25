
from tornado.web import RequestHandler

class FiferHandler(RequestHandler):

    def get(self, path):
        self.write('<h1>Fifer goes here!</h1>')
        self.write('<p>...</p>')
