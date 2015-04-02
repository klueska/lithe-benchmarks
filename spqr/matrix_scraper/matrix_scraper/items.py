# -*- coding: utf-8 -*-

# Define here the models for your scraped items
#
# See documentation in:
# http://doc.scrapy.org/en/latest/topics/items.html

from scrapy import Spider, Item, Field

class MatrixGroup(Item):
    url = Field()
    name = Field()
    num_matrices = Field()
    description = Field()
    details_url = Field()
    matrices = Field()

class Matrix(Item):
    url = Field()
    group = Field()
    name = Field()
    id = Field()
    matlab_file = Field()
    mm_file = Field()
    rb_file = Field()
    num_rows = Field()
    num_cols= Field()
    num_nonzeros = Field()
    type = Field()
    sym = Field()
    spd = Field()
    
