from scrapy import Spider, Request
from matrix_scraper.items import MatrixGroup, Matrix
from urlparse import urljoin
from locale import setlocale, LC_ALL, atoi
setlocale( LC_ALL, 'en_US.UTF-8' ) 

class MatrixGroupSpider(Spider):
	name = 'matrix-groups'
	allowed_domains = ['cise.ufl.edu']
	start_urls = ['http://www.cise.ufl.edu/research/sparse/matrices/groups.html']

	def parse(self, response):
		for row in response.xpath('//table/tr'):
			base_url = response.url
			mg = MatrixGroup()
			tds = row.xpath('td')
			matrices_url = tds[0].xpath('a/@href').extract()[0]
			details_url = tds[2].xpath('a/@href').extract()[0]
			mg['url'] = urljoin(base_url, matrices_url)
			mg['name'] = tds[0].xpath('a/text()').extract()[0].strip()
			mg['num_matrices'] = atoi(tds[1].xpath('text()').extract()[0])
			mg['description'] = tds[3].xpath('text()').extract()[0].strip()
			mg['details_url'] = urljoin(base_url, details_url)
			yield self.set_field(base_url, matrices_url, tds, mg, self.parse_matrices)

	def set_field(self, base_url, url, tds, mg, callback):
		url = urljoin(base_url, url)
		return Request(url, callback=callback, meta={
			'base_url': base_url,
			'tds': tds,
			'mg': mg
		})

	def parse_matrices(self, response):
		base_url = response.url
		mg = response.request.meta['mg']
		mg['matrices'] = []
		for row in response.xpath('//table/tr')[1:]:
			m = Matrix()
			tds = row.xpath('td')
			files = tds[4].xpath('a/@href').extract()
			url = tds[0].xpath('a/@href').extract()[0]
			m['url'] = urljoin(base_url, url)
			m['group'] = tds[2].xpath('text()').re(r'(.*)\/')[0].strip()
			m['name'] = tds[2].xpath('a/text()').extract()[0].strip()
			m['id'] = tds[3].xpath('text()').extract()[0].strip()
			m['matlab_file'] = urljoin(base_url, files[0])
			m['mm_file'] = urljoin(base_url, files[1])
			m['rb_file'] = urljoin(base_url, files[2])
			m['num_rows'] = atoi(tds[5].xpath('text()').extract()[0])
			m['num_cols'] = atoi(tds[6].xpath('text()').extract()[0])
			m['num_nonzeros'] = atoi(tds[7].xpath('text()').extract()[0])
			m['type'] = tds[8].xpath('text()').extract()[0].strip()
			m['sym'] = tds[9].xpath('text()').extract()[0].strip()
			m['spd'] = tds[10].xpath('text()').extract()[0].strip()
			mg['matrices'].append(m)
		return mg
		
