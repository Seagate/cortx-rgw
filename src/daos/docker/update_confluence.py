from bs4 import BeautifulSoup   # pip3 install beautifulsoup4
import json
import requests  # pip3 install requests
import base64
from requests.auth import HTTPBasicAuth
import copy
import csv
import sys
import os

# https://developer.atlassian.com/cloud/confluence/rest/api-group-content/#api-wiki-rest-api-content-id-put
auth_email = os.environ['CONFLUENCE_UPDATE_EMAIL']
auth_key = os.environ['CONFLUENCE_UPDATE_KEY']
auth_str = auth_email + ":" + auth_key
auth_bytes = auth_str.encode('ascii')
auth_encoded = base64.b64encode(auth_bytes)
auth_basic = auth_encoded.decode('ascii')
auth_token = "Basic " + auth_basic


def create_payload(json_page):
   payload_json = {
       "version": {
           "number": 5
       },
       "title": "<string>",
       "type": "<string>",
       "status": "current",
       "space": {
           "id": 458755,
           "key": "PUB",
       },
       "body": {
           "storage": {
               "value": "<string>",
               "representation": "storage"
           },
       }
   }
   payload_json["version"]["number"] = page_json["version"]["number"]
   payload_json["space"]["id"] = page_json["space"]["id"]
   payload_json["space"]["key"] = page_json["space"]["key"]
   payload_json["title"] = page_json["title"]
   payload_json["type"] = page_json["type"]
   payload_json["status"] = page_json["status"]
   payload_json["body"]["storage"]["value"] = page_json["body"]["storage"]["value"]
   return payload_json


def update_page(payload):
   new_payload = json.dumps(create_payload(payload))
   updateurl = "https://seagate-systems.atlassian.net/wiki/rest/api/content/1069023297"
   updateheaders = {
       "Accept": "application/json",
       "Content-Type": "application/json",
       "Authorization": auth_token
   }
   response = requests.request(
       "PUT", updateurl, data=new_payload, headers=updateheaders)
   return response


def get_space():
   geturl = "https://seagate-systems.atlassian.net/wiki/rest/api/space"
   getheaders = {
       "Accept": "application/json",
       "Authorization": auth_str
   }
   response = requests.request("GET", geturl, headers=getheaders)
   page_json = response.text
   page_json = json.loads(response.text)
   return page_json


def get_page_json():
   geturl = "https://seagate-systems.atlassian.net/wiki/rest/api/content/1069023297?expand=body.storage,version,space"
   getheaders = {
       "Accept": "application/json",
       "Authorization": auth_str
   }
   response = requests.request("GET", geturl, headers=getheaders)
   page_json = response.text
   page_json = json.loads(response.text)
   return page_json


def update_table_add_csv(table, csv_data):
   tablerow = copy.copy(table.tr)
   table_headers = tablerow.find_all("th")
   for header in table_headers:
      header.name = "td"  # replaces th tag with td

   strong_tags = tablerow.find_all("strong")
   for match in strong_tags:
      match.replaceWithChildren()

   tablecolumn = tablerow.find('td')
   for columndata in csv_data:
      tablecolumn.string.replace_with(columndata)
      tablecolumn = tablecolumn.next_sibling
   table.tr.insert_after(tablerow)


def get_csv_data(filename):
   s3_summary_csv = open(filename)
   s3_summary_reader = csv.reader(s3_summary_csv, delimiter=',')
   csv_header = next(s3_summary_reader)
   csv_data = next(s3_summary_reader)
   return csv_data

page_json = get_page_json()
csv_data = get_csv_data(sys.argv[1])
soup = BeautifulSoup(page_json["body"]["storage"]["value"], 'html.parser')
table = soup.tbody
tableheader = table.tr.find_all('th')
update_table_add_csv(table, csv_data)
page_json["body"]["storage"]["value"] = str(soup)
page_json["version"]["number"] += 1
payload = page_json
results = update_page(payload)
print(results)
